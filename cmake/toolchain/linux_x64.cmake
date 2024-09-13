## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(cross_prefix $ENV{TOOLCHAIN_DIR}/bin/x86_64-linux-gnu)
set(COMPILE_TARGET x86_64-nx-linux-gnu)
include(${CMAKE_CURRENT_LIST_DIR}/common/linux.cmake)
