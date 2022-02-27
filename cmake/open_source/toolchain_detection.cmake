## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This file is specific for open directory and must not be used in main project.

option(useClang "Use clang to compile the project." OFF)
if(useClang)
    set(ENV{USE_CLANG} ON)
endif()

if(toolchainDir) # Parameter overrides environment variable.
    set(ENV{TOOLCHAIN_DIR} "${toolchainDir}")
endif()

if(NOT CMAKE_TOOLCHAIN_FILE AND targetDevice)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchain/${targetDevice}.cmake")
    if(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})
        unset(CMAKE_TOOLCHAIN_FILE)
    endif()
endif()
