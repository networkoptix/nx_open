## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.14)

project(rpi_camera_plugin)

set(rpiFirmware "" CACHE STRING
    "Path to Raspberry Pi libraries containing bcm_host and openmaxil.")

if("${rpiFirmware}" STREQUAL "")
    message(FATAL_ERROR "rpiFirmware not defined or empty.")
endif()

#--------------------------------------------------------------------------------------------------

# Define nx_kit lib, static.
set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "NO" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../../nx_kit ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*)
add_library(rpi_camera_plugin SHARED ${source_files})

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(rpi_camera_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)

target_link_libraries(rpi_camera_plugin PRIVATE
    nx_kit
    bcm_host
    openmaxil
)

target_include_directories(rpi_camera_plugin PRIVATE
    ../../../src #< SDK headers.
    SYSTEM ${rpiFirmware}/opt/vc/include
)

target_link_directories(rpi_camera_plugin PRIVATE
    ${rpiFirmware}/opt/vc/lib
)

set_target_properties(rpi_camera_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
