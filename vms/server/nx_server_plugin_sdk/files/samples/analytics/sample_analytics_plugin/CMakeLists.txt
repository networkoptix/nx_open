## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#--------------------------------------------------------------------------------------------------
# Define sample_analytics_plugin lib, dynamic, depends on nx_kit and nx_sdk.

set(SAMPLE_ANALYTICS_PLUGIN_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
file(GLOB_RECURSE SAMPLE_ANALYTICS_PLUGIN_SRC CONFIGURE_DEPENDS
    ${SAMPLE_ANALYTICS_PLUGIN_SRC_DIR}/*)

add_library(sample_analytics_plugin SHARED ${SAMPLE_ANALYTICS_PLUGIN_SRC})
target_include_directories(sample_analytics_plugin PRIVATE ${SAMPLE_ANALYTICS_PLUGIN_SRC_DIR})
target_link_libraries(sample_analytics_plugin PRIVATE nx_kit nx_sdk)

target_compile_definitions(sample_analytics_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
)
