cmake_minimum_required(VERSION 3.7)
project(nx_kit_test)

set(CMAKE_CXX_STANDARD 11)

include_directories(src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -Wall \
    -pedantic \
")

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(EXPORT __attribute__\(\(visibility\("default"\)\)\))

set(NX_KIT_SRC
    src/nx/kit/ini_config.h
    src/nx/kit/ini_config.cpp
    src/nx/kit/debug.h
    src/nx/kit/debug.cpp
    src/nx/kit/test.h
    src/nx/kit/test.cpp
)

add_library(nx_kit SHARED ${NX_KIT_SRC})
target_compile_definitions(nx_kit
    PRIVATE NX_KIT_API=${EXPORT}
    INTERFACE NX_KIT_API=
)

add_library(nx_kit_disabled_ini_config SHARED ${NX_KIT_SRC})
target_compile_definitions(nx_kit_disabled_ini_config
    PRIVATE NX_KIT_API=${EXPORT}
    PRIVATE NX_INI_CONFIG_DISABLED
    INTERFACE NX_KIT_API=
)

set(TEST_SRC
        tests/ini_config_test.cpp
        tests/debug_test.cpp
        tests/main.cpp
)

add_executable(nx_kit_test ${TEST_SRC})
target_link_libraries(nx_kit_test nx_kit)

add_executable(nx_kit_disabled_ini_config_test ${TEST_SRC})
target_link_libraries(nx_kit_disabled_ini_config_test nx_kit_disabled_ini_config)
