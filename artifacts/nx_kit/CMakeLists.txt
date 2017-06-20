# Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
cmake_minimum_required(VERSION 3.3.2)
project(nx_kit_test)

set(CMAKE_CXX_STANDARD 11)

include_directories(src)

set(BUILD_SHARED_LIBS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -Wall \
    -pedantic \
")

include(nx_kit.cmake)

# Test for ini_config disabled via -DNX_INI_CONFIG_DISABLED.
add_library(test_disabled_ini_config
    src/nx/kit/ini_config.cpp
    src/nx/kit/test.cpp
    tests/ini_config_test.cpp
    tests/test_disabled_ini_config.cpp
)
target_compile_definitions(test_disabled_ini_config
    PRIVATE NX_INI_CONFIG_DISABLED
    PRIVATE TEST_DISABLED_INI_CONFIG_API=${NX_KIT_API_EXPORT_MACRO}
    INTERFACE TEST_DISABLED_INI_CONFIG_API=${NX_KIT_API_IMPORT_MACRO}
    PRIVATE NX_KIT_API= #< nx_kit sources are statically linked into lib test_disabled_ini_config.
)

# Executable for all tests.
add_executable(nx_kit_test
    tests/ini_config_test.cpp
    tests/debug_test.cpp
    tests/ini.cpp
    tests/test_ini_config_c.c
    tests/main.cpp
)
target_link_libraries(nx_kit_test
    nx_kit
    test_disabled_ini_config
)
