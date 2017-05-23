cmake_minimum_required(VERSION 3.7)
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
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(EXPORT_MACRO "__attribute__((visibility(\"default\")))")
target_compile_definitions(test_disabled_ini_config
    PRIVATE NX_INI_CONFIG_DISABLED
    PRIVATE TEST_DISABLED_INI_CONFIG_API=${EXPORT_MACRO}
    INTERFACE TEST_DISABLED_INI_CONFIG_API=
    PRIVATE NX_KIT_API=
    INTERFACE NX_KIT_API=
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
