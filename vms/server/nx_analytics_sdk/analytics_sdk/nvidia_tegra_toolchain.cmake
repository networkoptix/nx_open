## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(CMAKE_C_COMPILER "/usr/bin/aarch64-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/aarch64-linux-gnu-g++")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "" FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
