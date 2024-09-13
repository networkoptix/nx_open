## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(COMPILE_TARGET aarch64-nx-linux-gnu)
set(cross_prefix $ENV{TOOLCHAIN_DIR}/bin/aarch64-linux-gnu)
include(${CMAKE_CURRENT_LIST_DIR}/common/linux.cmake)
