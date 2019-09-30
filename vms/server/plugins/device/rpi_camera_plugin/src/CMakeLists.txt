## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.1)

project(rpi_camera_plugin)

set(rpiSysroot "" CACHE STRING
    "Path to Raspberry Pi sysroot containing libs bcm_host and openmaxil.")
    
if("${rpiSysroot}" STREQUAL "")
    message(FATAL_ERROR "rpiSysroot not defined or empty.")
endif()

#--------------------------------------------------------------------------------------------------

# Define nx_kit lib, static.
set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "NO" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../../nx_kit ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

file(GLOB_RECURSE source_files ${CMAKE_CURRENT_SOURCE_DIR}/*)
add_library(rpi_camera_plugin SHARED ${source_files})

target_link_libraries(rpi_camera_plugin PRIVATE
    nx_kit
    bcm_host
    openmaxil
)

target_include_directories(rpi_camera_plugin PRIVATE
    ../../../src #< SDK headers.
    SYSTEM ${rpiSysroot}/opt/vc/include
)

target_link_directories(rpi_camera_plugin PRIVATE
    ${rpiSysroot}/opt/vc/lib
)

set_target_properties(rpi_camera_plugin PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

