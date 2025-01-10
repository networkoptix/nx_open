## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if("${COMPILE_TARGET}" STREQUAL "")
    message(FATAL_ERROR "COMPILE_TARGET must be set before including this file.")
endif()

set(CMAKE_SYSTEM_NAME Windows)

if("$ENV{USE_CLANG}")
    if("$ENV{CLANG_DIR}" STREQUAL "")
        message(FATAL_ERROR
            "CLANG_DIR environment variable is not set. Check cmake/conan_dependencies.cmake.")
    endif()

    include($ENV{CLANG_DIR}/toolchains/${COMPILE_TARGET}.cmake)

    set(CMAKE_CUDA_COMPILER "$ENV{CLANG_DIR}/bin/clang++.exe")
    set(CMAKE_CUDA_COMPILER_TOOLKIT_ROOT "$ENV{CUDA_PATH}")
    set(CMAKE_CUDA_FLAGS "--cuda-path=$ENV{CUDA_PATH} --cuda-gpu-arch=sm_60")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        string(APPEND CMAKE_CUDA_FLAGS " -D_DEBUG -D_DLL -D_MT ")
        set(_crt_libs "/DEFAULTLIB:msvcrtd.lib /DEFAULTLIB:ucrtd.lib /DEFAULTLIB:vcruntimed.lib")
    else()
        set(_crt_libs "/DEFAULTLIB:msvcrt.lib /DEFAULTLIB:ucrt.lib /DEFAULTLIB:vcruntime.lib")
    endif()
    set(CMAKE_CUDA_HOST_LINK_LAUNCHER "$ENV{CLANG_DIR}/bin/clang-cl.exe -fuse-ld=lld")
    set(CMAKE_CUDA_COMPILER_ID Clang)
    set(_CMAKE_CUDA_WHOLE_FLAG "-c")
    set(CMAKE_CUDA_COMPILE_OBJECT "${CMAKE_CUDA_COMPILER} ${CMAKE_CUDA_FLAGS} ${_CMAKE_CUDA_WHOLE_FLAG} <SOURCE> -o <OBJECT>")
    set(CMAKE_CUDA_LINK_EXECUTABLE "${CMAKE_CUDA_HOST_LINK_LAUNCHER} \
        <OBJECTS> \
        /link <LINK_FLAGS> \
        /OUT:<TARGET> \
        /DEFAULTLIB:kernel32.lib \
        /DEFAULTLIB:user32.lib \
        ${_crt_libs} \
        <LINK_LIBRARIES>${__IMPLICIT_LINKS}")
endif()
