#!/bin/sh

COPTS="--with-tests=regular --enable-harfbuzz --enable-liblz4 --enable-image-loader-webp --enable-xinput22 --enable-xine --enable-multisense --enable-lua-old --enable-xpresent --enable-hyphen"
PARALLEL_JOBS=10

# --enable-libvlc --enable-vnc-server --enable-g-main-loop --enable-libuv --enable-fb --enable-eglfs --enable-sdl --enable-gl-drm --enable-egl --enable-pixman --enable-tile-rotate --enable-ecore-buffer --enable-image-loader-generic --enable-image-loader-jp2k --enable-gesture --enable-v4l2

# --with-profile=PROFILE --with-crypto=CRYPTO

# Normal build test of all targets
./autogen.sh $COPTS $@
make -j $PARALLEL_JOBS
make -j $PARALLEL_JOBS examples
