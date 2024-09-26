## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(ftplibpp_dir ${CMAKE_CURRENT_SOURCE_DIR}/ftplibpp)
set(ftplibpp_source_files
    ${ftplibpp_dir}/ftplibpp/ftplib.cpp
)

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
add_library(ftp_storage_plugin SHARED ${source_files} ${ftplibpp_source_files})

target_link_libraries(ftp_storage_plugin PRIVATE nx_sdk)

target_include_directories(ftp_storage_plugin PRIVATE
    ${ftplibpp_dir}
)

target_compile_definitions(ftp_storage_plugin
    PRIVATE
        # Needed for ftplib.
        -DNOSSL
        -DNOLFS
        NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE
        NX_PLUGIN_API=${API_IMPORT_MACRO}
)

if(WIN32)
    target_compile_definitions(ftp_storage_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(ftp_storage_plugin PRIVATE ws2_32)
endif()

set_target_properties(ftp_storage_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
