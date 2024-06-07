## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(NOT "$ENV{ANDROID_NDK}" STREQUAL "")
    set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK}")
else()
    set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_DIR}")
endif()

if(NOT EXISTS ${CMAKE_ANDROID_NDK})
    message(FATAL_ERROR "Android NDK directory does not exist: ${CMAKE_ANDROID_NDK}")
endif()

set(ANDROID_NATIVE_API_LEVEL 21)
set(ANDROID_PLATFORM ${ANDROID_NATIVE_API_LEVEL})
set(ANDROID_STL c++_shared)
set(ANDROID_TOOLCHAIN clang)

include(${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake)
