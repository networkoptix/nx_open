## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*)

add_library(sample_cloud_storage_plugin SHARED
    ${source_files}
    ${sdk_source_files}
    ${nx_kit_source_files}
)

target_link_libraries(sample_cloud_storage_plugin PRIVATE nx_kit nx_sdk)

target_compile_definitions(sample_cloud_storage_plugin
    PRIVATE
        -DNOSSL
        -DNOLFS
        -DNX_KIT_API=
        NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE
        NX_PLUGIN_API=${API_IMPORT_MACRO}
)

if(WIN32)
    target_compile_definitions(sample_cloud_storage_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(sample_cloud_storage_plugin PRIVATE ws2_32)
endif()

set_target_properties(sample_cloud_storage_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
