## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(CMAKE_SYSTEM_PROCESSOR arm)
set(COMPILE_TARGET arm-nx-linux-gnueabihf)
set(cross_prefix $ENV{TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf)
include(${CMAKE_CURRENT_LIST_DIR}/common/linux.cmake)

set(flags " -mcpu=cortex-a7 -mfpu=neon-vfpv4")
string(APPEND CMAKE_C_FLAGS ${flags})
string(APPEND CMAKE_CXX_FLAGS ${flags})
unset(flags)
