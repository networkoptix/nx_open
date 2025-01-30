## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(src_dir ${CMAKE_CURRENT_LIST_DIR}/src)
file(GLOB_RECURSE src_files CONFIGURE_DEPENDS ${src_dir}/*)

# Used in Combined Plugin.
add_library(sample_analytics_plugin_integration STATIC ${src_files})
target_include_directories(sample_analytics_plugin_integration PUBLIC ${src_dir})
target_link_libraries(sample_analytics_plugin_integration PUBLIC nx_sdk)

add_library(sample_analytics_plugin SHARED "plugin/plugin.cpp")
target_link_libraries(sample_analytics_plugin PRIVATE sample_analytics_plugin_integration)
