## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.14)

project(image_library_plugin)

# Define nx_kit lib, static.
set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "NO" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../../../nx_kit ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*)
add_library(image_library_plugin SHARED ${source_files})

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(image_library_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)

target_link_libraries(image_library_plugin PRIVATE nx_kit)

target_include_directories(image_library_plugin PRIVATE ../../../../src) #< SDK headers.

# Use all CPU cores by MSVC.
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /MP")
    # Do not create separate .pdb files for object files (workaround for mspdbsrv.exe bug, needed
    # for using "/MP" flag).
    target_compile_options(image_library_plugin BEFORE PRIVATE /Z7)
endif()

if(WIN32)
    target_compile_definitions(image_library_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(image_library_plugin PRIVATE -Wno-deprecated-declarations)
endif()

set_target_properties(image_library_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
