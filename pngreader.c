/*
  pngreader.c: PNG image reader for VapourSynth

  This file is a part of vspngreader

  Copyright (C) 2012  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with Libav; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "png.h"
#include "VapourSynth.h"

#define VS_PNGR_VERSION "0.1.0"
#define PNG_SIG_LENGTH 8
#define PNG_TRANSFORM_SETTINGS \
    (PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_SWAP_ENDIAN)


typedef struct png_handle png_hnd_t;
struct png_handle {
    const char **src_files;
    png_bytepp decode_buff;
    png_size_t row_size;
    VSVideoInfo vi;
    void (VS_CC *write_frame)(png_hnd_t *, VSFrameRef *, const VSAPI *);
};

typedef struct {
    const VSMap *in;
    VSMap *out;
    VSCore *core;
    const VSAPI *vsapi;
} vs_args_t;

typedef struct {
    png_uint_32 width;
    png_uint_32 height;
    VSPresetFormat dst_format;
    png_size_t row_size;
} png_props_t;


static int VS_CC
read_png(png_hnd_t *ph, int n)
{
    png_structp p_str =
        png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!p_str) {
        return -1;
    }

    png_infop p_info = png_create_info_struct(p_str);
    if (!p_info) {
        png_destroy_read_struct(&p_str, NULL, NULL);
        return -1;
    }

    FILE *fp = fopen(ph->src_files[n], "rb");
    if (!fp) {
        return -1;
    }

    png_init_io(p_str, fp);
    png_set_rows(p_str, p_info, ph->decode_buff);
    png_read_png(p_str, p_info, PNG_TRANSFORM_SETTINGS, NULL);

    fclose(fp);
    png_destroy_read_struct(&p_str, &p_info, NULL);

    return 0;
}


static void VS_CC
write_gray_frame(png_hnd_t *ph, VSFrameRef *dst, const VSAPI *vsapi)
{
    uint8_t *srcp = (uint8_t *)ph->decode_buff[0];
    int row_size = ph->row_size;
    int height = vsapi->getFrameHeight(dst, 0);
    uint8_t *dstp = vsapi->getWritePtr(dst, 0);
    int dst_stride = vsapi->getStride(dst, 0);
    if (row_size == dst_stride) {
        memcpy(dstp, srcp, row_size * height);
        return;
    }

    for (int i = 0; i < height; i++) {
        memcpy(dstp, srcp, row_size);
        dstp += dst_stride;
        srcp += row_size;
    }
}


static inline uint32_t VS_CC
bitor8to32(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
           ((uint32_t)b2 << 8) | (uint32_t)b3;
}


static void VS_CC
write_rgb24_frame(png_hnd_t *ph, VSFrameRef *dst, const VSAPI *vsapi)
{
    typedef struct {
        uint8_t c[12];
    } rgb24_t;

    uint8_t *srcp_orig = ph->decode_buff[0];
    int row_size = (ph->vi.width + 3) >> 2;
    int height = ph->vi.height;
    int src_stride = ph->row_size;

    uint32_t *dstp0 = (uint32_t *)vsapi->getWritePtr(dst, 0);
    uint32_t *dstp1 = (uint32_t *)vsapi->getWritePtr(dst, 1);
    uint32_t *dstp2 = (uint32_t *)vsapi->getWritePtr(dst, 2);
    int dst_stride = vsapi->getStride(dst, 0) >> 2;

    for (int y = 0; y < height; y++) {
        rgb24_t *srcp = (rgb24_t *)(srcp_orig + y * src_stride);
        for (int x = 0; x < row_size; x++) {
            dstp0[x] = bitor8to32(srcp[x].c[9], srcp[x].c[6],
                                  srcp[x].c[3], srcp[x].c[0]);
            dstp1[x] = bitor8to32(srcp[x].c[10], srcp[x].c[7],
                                  srcp[x].c[4], srcp[x].c[1]);
            dstp2[x] = bitor8to32(srcp[x].c[11], srcp[x].c[8],
                                  srcp[x].c[5], srcp[x].c[2]);
        }
        dstp0 += dst_stride;
        dstp1 += dst_stride;
        dstp2 += dst_stride;
    }
}


static void VS_CC
write_rgb48_frame(png_hnd_t *ph, VSFrameRef *dst, const VSAPI *vsapi)
{
    typedef struct {
        uint16_t c[3];
    } rgb48_t;

    rgb48_t *srcp = (rgb48_t *)(ph->decode_buff[0]);
    int width = ph->vi.width;
    int height = ph->vi.height;

    uint16_t *dstp0 = (uint16_t *)vsapi->getWritePtr(dst, 0);
    uint16_t *dstp1 = (uint16_t *)vsapi->getWritePtr(dst, 1);
    uint16_t *dstp2 = (uint16_t *)vsapi->getWritePtr(dst, 2);
    int stride = vsapi->getStride(dst, 0) >> 1;;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dstp0[x] = srcp[x].c[0];
            dstp1[x] = srcp[x].c[1];
            dstp2[x] = srcp[x].c[2];
        }
        srcp += width;
        dstp0 += stride;
        dstp1 += stride;
        dstp2 += stride;
    }
}


static const VSFrameRef * VS_CC
png_get_frame(int n, int activation_reason, void **instance_data,
              void **frame_data, VSFrameContext *frame_ctx, VSCore *core,
              const VSAPI *vsapi)
{
    if (activation_reason != arInitial) {
        return NULL;
    }

    png_hnd_t *ph = (png_hnd_t *)*instance_data;

    int frame_number = n;
    if (n >= ph->vi.numFrames) {
        frame_number = ph->vi.numFrames - 1;
    }

    if (read_png(ph, frame_number)) {
        return NULL;
    }

    VSFrameRef *dst = vsapi->newVideoFrame(ph->vi.format, ph->vi.width,
                                           ph->vi.height, NULL, core);

    VSMap *props = vsapi->getFramePropsRW(dst);
    vsapi->propSetInt(props, "_DurationNum", ph->vi.fpsDen, 0);
    vsapi->propSetInt(props, "_DurationDen", ph->vi.fpsNum, 0);

    ph->write_frame(ph, dst, vsapi);

    return dst;
}


static void close_handler(png_hnd_t *ph)
{
    if (!ph) {
        return;
    }
    if (ph->decode_buff[0]) {
        free(ph->decode_buff[0]);
    }
    if (ph->decode_buff) {
        free(ph->decode_buff);
    }
    if (ph->src_files) {
        free(ph->src_files);
    }
    free(ph);
}


static void VS_CC
vs_init(VSMap *in, VSMap *out, void **instance_data, VSNode *node,
        VSCore *core, const VSAPI *vsapi)
{
    png_hnd_t *ph = (png_hnd_t *)*instance_data;
    vsapi->setVideoInfo(&ph->vi, node);
}


static void VS_CC
vs_close(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
    png_hnd_t *ph = (png_hnd_t *)instance_data;
    close_handler(ph);
}


#define COLOR_OR_BITS(color, bits) \
    (((uint32_t)color << 16) | (uint32_t)bits)
static VSPresetFormat VS_CC
get_dst_format(int color_type, int bits)
{
    uint32_t p_color = COLOR_OR_BITS(color_type, bits);
    const struct {
        uint32_t png_color_type;
        VSPresetFormat vsformat;
    } table[] = {
        { COLOR_OR_BITS(PNG_COLOR_TYPE_GRAY,  8),  pfGray8  },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_GRAY, 16),  pfGray16 },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_RGB,   8),  pfRGB24  },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_RGB,  16),  pfRGB48  },
        { p_color, pfNone }
    };

    int i = 0;
    while (table[i].png_color_type != p_color) i++;
    return table[i].vsformat;
}
#undef COLOR_OR_BITS


static char * VS_CC
check_srcs(png_hnd_t *ph, int n, png_props_t *png_props)
{
    FILE *fp = fopen(ph->src_files[n], "rb");
    if (!fp) {
        return "failed to open source file";
    }

    uint8_t signature[PNG_SIG_LENGTH];
    fread(signature, 1, PNG_SIG_LENGTH, fp);
    if (png_sig_cmp(signature, 0, PNG_SIG_LENGTH)) {
        fclose(fp);
        return "not png image";
    }

    png_structp p_str =
        png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!p_str) {
        fclose(fp);
        return "failed to create png_read_struct";
    }

    png_infop p_info = png_create_info_struct(p_str);
    if (!p_info) {
        fclose(fp);
        png_destroy_read_struct(&p_str, NULL, NULL);
        return "failed to create png_info_struct";
    }

    png_init_io(p_str, fp);
    png_set_sig_bytes(p_str, PNG_SIG_LENGTH);
    png_read_info(p_str, p_info);

    png_uint_32 width, height;
    int color_type, bit_depth;

    png_get_IHDR(p_str, p_info, &width, &height, &bit_depth, &color_type,
                 NULL, NULL, NULL);
    if (color_type & PNG_COLOR_MASK_ALPHA) {
        png_set_strip_alpha(p_str);
    }
    if (color_type & PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(p_str);
    }
    if (bit_depth < 8) {
        png_set_packing(p_str);
    }
    png_read_update_info(p_str, p_info);
    png_get_IHDR(p_str, p_info, &width, &height, &bit_depth, &color_type,
                 NULL, NULL, NULL);
    png_size_t row_size = png_get_rowbytes(p_str, p_info);

    fclose(fp);
    png_destroy_read_struct(&p_str, &p_info, NULL);

    VSPresetFormat dst_format = get_dst_format(color_type, bit_depth);
    if (dst_format == pfNone) {
        return "unsupported png color type";
    }

    if (png_props->width != width || png_props->height != height) {
        if (n > 0) {
            return "found a file which has diffrent resolution from the first one";
        }
        png_props->width = width;
        png_props->height = height;
    }

    if (png_props->dst_format != dst_format) {
        if (n > 0) {
            return "found a file which color does not match the first one";
        }
        png_props->dst_format = dst_format;
    }

    if (png_props->row_size != row_size) {
        if (n > 0) {
            return "found a file which row size does not match the first one";
        }
        png_props->row_size = row_size;
    }

    return NULL;
}


static int VS_CC allocate_decode_buffer(png_hnd_t *ph)
{
    int height = ph->vi.height;
    ph->decode_buff = (png_bytepp)calloc(sizeof(png_bytep), height);
    if (!ph->decode_buff) {
        return -1;
    }

    png_bytep buff =
        (png_bytep)calloc(sizeof(png_byte), ph->row_size * height + 32);
    if (!buff) {
        return -1;
    }

    for (int y = 0; y < height; y++) {
        ph->decode_buff[y] = buff;
        buff += ph->row_size;
    }

    return 0;
}


static void VS_CC
set_args_int64(int64_t *p, int default_value, const char *arg, vs_args_t *va)
{
    int err;
    *p = va->vsapi->propGetInt(va->in, arg, 0, &err);
    if (err) {
        *p = default_value;
    }
}


#define RET_IF_ERR(cond, ...) \
{\
    if (cond) {\
        close_handler(ph);\
        snprintf(msg, 240, __VA_ARGS__);\
        vsapi->setError(out, msg_buff);\
        return;\
    }\
}
static void VS_CC
create_reader(const VSMap *in, VSMap *out, void *user_data, VSCore *core,
              const VSAPI *vsapi)
{
    char msg_buff[256] = "pngr: ";
    char *msg = msg_buff + strlen(msg_buff);
    vs_args_t va = {in, out, core, vsapi};

    png_hnd_t *ph = (png_hnd_t *)calloc(sizeof(png_hnd_t), 1);
    RET_IF_ERR(!ph, "failed to create handler");

    int num_srcs = vsapi->propNumElements(in, "files");
    RET_IF_ERR(num_srcs < 1, "no source file");

    ph->src_files = (const char **)calloc(sizeof(char *), num_srcs);
    RET_IF_ERR(!ph->src_files, "failed to allocate array of src files");

    int err;
    for (int i = 0; i < num_srcs; i++) {
        ph->src_files[i] = vsapi->propGetData(in, "files", i, &err);
        RET_IF_ERR(err || strlen(ph->src_files[i]) == 0,
                   "zero length file name was found");
    }

    png_props_t png_props = { 0 };
    for (int i = 0; i < num_srcs; i++) {
        char *cs = check_srcs(ph, i, &png_props);
        RET_IF_ERR(cs, "file %d: %s", i, cs);
    }

    ph->vi.width = png_props.width;
    ph->vi.height = png_props.height;
    ph->vi.numFrames = num_srcs;
    ph->vi.format = vsapi->getFormatPreset(png_props.dst_format, core);
    set_args_int64(&ph->vi.fpsNum, 24, "fpsnum", &va);
    set_args_int64(&ph->vi.fpsDen, 1, "fpsden", &va);
    ph->row_size = png_props.row_size;

    switch (ph->vi.format->id) {
    case pfGray8:
    case pfGray16:
        ph->write_frame = write_gray_frame;
        break;
    case pfRGB48:
        ph->write_frame = write_rgb48_frame;
        break;
    default:
        ph->write_frame = write_rgb24_frame;
    }

    RET_IF_ERR(allocate_decode_buffer(ph), "failed to allocate decode buffer");

    const VSNodeRef *node =
        vsapi->createFilter(in, out, "Read", vs_init, png_get_frame, vs_close,
                            fmSerial, 0, ph, core);

    vsapi->propSetNode(out, "clip", node, 0);
}
#undef RET_IF_ERR


VS_EXTERNAL_API(void) VapourSynthPluginInit(
    VSConfigPlugin f_config, VSRegisterFunction f_register, VSPlugin *plugin)
{
    f_config("chikuzen.does.not.have.his.own.domain.pngr", "pngr",
             "PNG image reader for VapourSynth " VS_PNGR_VERSION,
             VAPOURSYNTH_API_VERSION, 1, plugin);
    f_register("Read", "files:data[];fpsnum:int:opt;fpsden:int:opt",
               create_reader, NULL, plugin);
}
