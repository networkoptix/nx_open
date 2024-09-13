## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if("${COMPILE_TARGET}" STREQUAL "")
    message(FATAL_ERROR "COMPILE_TARGET must be set before including this file.")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILE_TARGET ${COMPILE_TARGET})
set(CMAKE_CXX_COMPILE_TARGET ${COMPILE_TARGET})

if("$ENV{TOOLCHAIN_DIR}" STREQUAL "")
    message(FATAL_ERROR
        "TOOLCHAIN_DIR environment variable is not set. Check cmake/conan_dependencies.cmake.")
endif()

if("$ENV{USE_CLANG}")
    if("$ENV{CLANG_DIR}" STREQUAL "")
        message(FATAL_ERROR
            "CLANG_DIR environment variable is not set. Check cmake/conan_dependencies.cmake.")
    endif()

    include($ENV{CLANG_DIR}/toolchains/${COMPILE_TARGET}.cmake)

    set(flags "--gcc-toolchain=$ENV{TOOLCHAIN_DIR}")
    string(APPEND flags " --prefix=$ENV{TOOLCHAIN_DIR}/${NX_SYSROOT_TARGET}/bin")
    string(APPEND flags " --sysroot=$ENV{TOOLCHAIN_DIR}/${NX_SYSROOT_TARGET}/sysroot")
    set(CMAKE_C_FLAGS ${flags})
    set(CMAKE_CXX_FLAGS ${flags})
    set(CMAKE_SHARED_LINKER_FLAGS ${flags})
    set(CMAKE_EXE_LINKER_FLAGS ${flags})
else()
    set(CMAKE_C_COMPILER "${cross_prefix}-gcc" CACHE INTERNAL "")
    set(CMAKE_CXX_COMPILER "${cross_prefix}-g++" CACHE INTERNAL "")
endif()

set(CMAKE_CUDA_HOST_COMPILER ${CMAKE_CXX_COMPILER})
set(CMAKE_CUDA_FLAGS
    "-allow-unsupported-compiler -forward-unknown-to-host-compiler ${CMAKE_CXX_FLAGS}")
