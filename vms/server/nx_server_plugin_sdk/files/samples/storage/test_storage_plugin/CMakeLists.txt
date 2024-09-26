## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
add_library(test_storage_plugin SHARED ${source_files})

target_compile_definitions(test_storage_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)
target_include_directories(test_storage_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(test_storage_plugin PRIVATE nx_sdk)

if(WIN32)
    target_compile_definitions(test_storage_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(test_storage_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
