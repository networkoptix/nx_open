## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.1)

project(ftp_storage_plugin)

file(GLOB_RECURSE source_files ${CMAKE_CURRENT_SOURCE_DIR}/*)
add_library(ftp_storage_plugin SHARED ${source_files})

target_include_directories(ftp_storage_plugin PRIVATE ../../../src) #< SDK headers.

target_compile_definitions(ftp_storage_plugin PRIVATE
    # Needed for ftplib.
    -DNOSSL
    -DNOLFS
)

if(WIN32)
    target_compile_definitions(ftp_storage_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(ftp_storage_plugin PRIVATE ws2_32)
endif()

set_target_properties(ftp_storage_plugin PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
