================================================
vspngreader - PNG image reader for VapourSynth
================================================

PNG(Portable Network Graphic) image reader for VapourSynth.

Usage:
------
    >>> import vapoursynth as vs
    >>> core = vs.Core()
    >>> core.std.LoadPlugin('/path/to/vspngreader.dll')

    - read single file:
    >>> clip = core.jpgr.Read(['/path/to/file.png'])

    - read two or more files:
    >>> srcs = ['/path/to/file1.png', '/path/to/file2.png', ... ,'/path/to/fileX.png']
    >>> clip = core.pngr.Read(srcs)

    - read image sequence:
    >>> import os
    >>> dir = '/path/to/the/directory/'
    >>> srcs = [dir + src for src in os.listdir(dir) if src.endswith('.png')]
    >>> clip = core.pngr.Read(srcs)

Note:
-----
    - 1/2/4bits samples will be expanded to 8bits.

    - All alpha channel data will be stripped.

    - When reading two or more images, all those width, height, and destination formats need to be the same.
    - MNG and JNG are unsupported.

How to compile:
---------------
    vspngreader requires libpng-1.2(1.2.50 or later is recomended).

    And, libpng requires zlib-1.0.4 or later(1.2.7 or later is recomended).

    Therefore, you have to install these libraries at first.

    If you have already installed them, type as follows.

    $ git clone git://github.com/chikuzen/vspngreader.git
    $ cd ./vspngreader
    $ ./configure
    $ make

    - Currentry, libpng-1.4/1.5 cannot be used for vspngreader.

Link:
-----
    vspngreader source code repository:

        https://github.com/chikuzen/vspngreader

    libpng.org:

        http://www.libpng.org/

    zlib:

        http://www.zlib.net/


Author: Oka Motofumi (chikuzen.mo at gmail dot com)
