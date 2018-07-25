# Copyright 2018 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
cmake_minimum_required(VERSION 3.3.2)
project(nx_kit_test)

set(CMAKE_CXX_STANDARD 11)

include_directories(src)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(nxKitLibraryType "SHARED" CACHE STRING "nx_kit library type (STATIC or SHARED or empty)")

add_library(nx_kit ${nxKitLibraryType}
    ${CMAKE_CURRENT_LIST_DIR}/src/nx/kit/debug.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/nx/kit/ini_config.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/nx/kit/test.cpp
)
target_include_directories(nx_kit
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)

set(NX_KIT_API_IMPORT_MACRO "")
set(NX_KIT_API_EXPORT_MACRO "")
if(WIN32)
    if (nxKitLibraryType STREQUAL "SHARED")
        set(NX_KIT_API_IMPORT_MACRO "__declspec(dllimport)")
        set(NX_KIT_API_EXPORT_MACRO "__declspec(dllexport)")
    endif()
else()
    set_target_properties(nx_kit PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON
    )
    set(NX_KIT_API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(nx_kit
    PRIVATE NX_KIT_API=${NX_KIT_API_EXPORT_MACRO}
    INTERFACE NX_KIT_API=${NX_KIT_API_IMPORT_MACRO}
)

set(nxKitWithTests "YES" CACHE STRING "Whether to build unit tests")
if(nxKitWithTests)
    add_subdirectory(unit_tests)
endif()
