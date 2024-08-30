## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.14)
project(ftp_storage_plugin)

set(ftplibpp_dir ${CMAKE_CURRENT_SOURCE_DIR}/../ftplibpp)
set(ftplibpp_source_files
    ${ftplibpp_dir}/ftplibpp/ftplib.cpp
)

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*)
add_library(ftp_storage_plugin SHARED ${source_files} ${ftplibpp_source_files})

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(ftp_storage_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)

target_include_directories(ftp_storage_plugin PRIVATE
    ../../../../src #< SDK headers.
    ${ftplibpp_dir}
)

target_compile_definitions(ftp_storage_plugin PRIVATE
    # Needed for ftplib.
    -DNOSSL
    -DNOLFS
)

# Use all CPU cores by MSVC.
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /MP")
    # Do not create separate .pdb files for object files (workaround for mspdbsrv.exe bug, needed
    # for using "/MP" flag).
    target_compile_options(ftp_storage_plugin BEFORE PRIVATE /Z7)
endif()

if(WIN32)
    target_compile_definitions(ftp_storage_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(ftp_storage_plugin PRIVATE ws2_32)
endif()

set_target_properties(ftp_storage_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
