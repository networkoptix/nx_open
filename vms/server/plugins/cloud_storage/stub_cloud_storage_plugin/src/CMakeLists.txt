## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.14)
project(stub_cloud_storage_plugin)

get_filename_component(sdk_path "../../../../src"
    REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

get_filename_component(nx_kit_path "../../../../nx_kit/src"
    REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE sdk_source_files CONFIGURE_DEPENDS ${sdk_path}/*.cpp)
file(GLOB_RECURSE nx_kit_source_files CONFIGURE_DEPENDS
    ${nx_kit_path}/json11/*
    ${nx_kit_path}/nx/kit/json.cpp
    ${nx_kit_path}/nx/kit/debug.cpp
    ${nx_kit_path}/nx/kit/utils.cpp
    ${nx_kit_path}/nx/kit/ini_config.cpp
)

add_library(stub_cloud_storage_plugin SHARED
    ${source_files}
    ${sdk_source_files}
    ${nx_kit_source_files}
)

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(stub_cloud_storage_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)

target_include_directories(stub_cloud_storage_plugin PRIVATE
    ${sdk_path}
    ${nx_kit_path}
)

target_compile_definitions(stub_cloud_storage_plugin PRIVATE
    -DNOSSL
    -DNOLFS
    -DNX_KIT_API=
)

# Use all CPU cores by MSVC.
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /MP")
    # Do not create separate .pdb files for object files (workaround for mspdbsrv.exe bug, needed
    # for using "/MP" flag).
    target_compile_options(stub_cloud_storage_plugin BEFORE PRIVATE /Z7)
endif()

if(WIN32)
    target_compile_definitions(stub_cloud_storage_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(stub_cloud_storage_plugin PRIVATE ws2_32)
endif()

set_target_properties(stub_cloud_storage_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
