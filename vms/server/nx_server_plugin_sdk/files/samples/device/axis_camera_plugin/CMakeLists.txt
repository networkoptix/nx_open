## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(CMAKE_AUTOMOC ON)

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
add_library(axis_camera_plugin SHARED ${source_files})

target_compile_definitions(axis_camera_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)

target_link_libraries(axis_camera_plugin PRIVATE
    Qt6::Core
    Qt6::Network
    nx_sdk
)

if(WIN32)
    target_compile_definitions(axis_camera_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(axis_camera_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
