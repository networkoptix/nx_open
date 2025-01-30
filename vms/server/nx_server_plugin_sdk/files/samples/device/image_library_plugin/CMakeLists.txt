## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(src_dir ${CMAKE_CURRENT_LIST_DIR}/src)
file(GLOB_RECURSE src_files CONFIGURE_DEPENDS ${src_dir}/*)

# Used in Combined Plugin.
add_library(image_library_plugin_integration STATIC ${src_files})
target_include_directories(image_library_plugin_integration PUBLIC ${src_dir})
target_link_libraries(image_library_plugin_integration PUBLIC nx_sdk)
if(WIN32)
    target_compile_definitions(image_library_plugin_integration PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(image_library_plugin_integration PRIVATE -Wno-deprecated-declarations)
endif()

add_library(image_library_plugin SHARED "plugin/plugin.cpp")
target_link_libraries(image_library_plugin PRIVATE image_library_plugin_integration)
