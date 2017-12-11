# Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
cmake_minimum_required(VERSION 3.3.2)
project(nx_kit_test)

set(CMAKE_CXX_STANDARD 11)

include_directories(src)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -Wall \
    -pedantic \
")

set(nxKitLibraryType "SHARED" CACHE STRING "" FORCE)
include(nx_kit.cmake)

# Test for ini_config disabled via -DNX_INI_CONFIG_DISABLED.
add_library(disabled_ini_config_ut SHARED
    src/nx/kit/ini_config.cpp
    src/nx/kit/test.cpp
    unit_tests/ini_config_ut.cpp
    unit_tests/disabled_ini_config_ut.cpp
)
target_compile_definitions(disabled_ini_config_ut
    PRIVATE NX_INI_CONFIG_DISABLED
    PRIVATE DISABLED_INI_CONFIG_UT_API=${NX_KIT_API_EXPORT_MACRO}
    INTERFACE DISABLED_INI_CONFIG_UT_API=${NX_KIT_API_IMPORT_MACRO}
    PRIVATE NX_KIT_API= #< nx_kit sources are statically linked into lib disabled_ini_config_ut.
)

# Executable for all tests.
add_executable(nx_kit_ut
    unit_tests/ini_config_ut.cpp
    unit_tests/debug_ut.cpp
    unit_tests/ini.cpp
    unit_tests/ini_config_c_ut.c
    unit_tests/main.cpp
)
target_link_libraries(nx_kit_ut
    nx_kit
    disabled_ini_config_ut
)
