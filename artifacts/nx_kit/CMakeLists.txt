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

get_target_property(NX_KIT_SOURCES nx_kit SOURCES)

add_library(nx_kit_disabled_ini_config ${NX_KIT_SOURCES})
target_compile_definitions(nx_kit_disabled_ini_config
    PRIVATE NX_INI_CONFIG_DISABLED
    PRIVATE NX_KIT_API=${NX_KIT_API_EXPORT_MACRO}
    INTERFACE NX_KIT_API=${NX_KIT_API_IMPORT_MACRO}
)

set(TEST_SOURCES
    tests/ini_config_test.cpp
    tests/debug_test.cpp
    tests/main.cpp
)

set(C_WRAPPER_TEST_SOURCES
    tests/ini.cpp
    tests/main.c
)

add_executable(nx_kit_test ${TEST_SOURCES})
target_link_libraries(nx_kit_test nx_kit)

add_executable(nx_kit_disabled_ini_config_test ${TEST_SOURCES})
target_link_libraries(nx_kit_disabled_ini_config_test nx_kit_disabled_ini_config)

add_executable(nx_kit_c_wrapper_test ${C_WRAPPER_TEST_SOURCES})
target_link_libraries(nx_kit_c_wrapper_test nx_kit)
