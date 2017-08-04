#!/bin/sh

COPTS="--with-tests=regular --enable-wayland --enable-elput --enable-drm"
PARALLEL_JOBS=10

# Normal build test of all targets
./autogen.sh $COPTS $@
make -j $PARALLEL_JOBS
make -j $PARALLEL_JOBS examples
