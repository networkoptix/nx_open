## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
add_library(image_library_plugin SHARED ${source_files})

target_compile_definitions(image_library_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)

target_link_libraries(image_library_plugin PRIVATE nx_kit nx_sdk)

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
