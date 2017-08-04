#!/bin/sh

COPTS="--with-tests=regular --enable-harfbuzz --enable-liblz4 --enable-image-loader-webp --enable-xinput22 --enable-xine --enable-multisense --enable-lua-old --enable-libvlc --enable-xpresent --enable-hyphen"
PARALLEL_JOBS=10

# Normal build test of all targets
./autogen.sh $COPTS $@
make -j $PARALLEL_JOBS
make -j $PARALLEL_JOBS examples
