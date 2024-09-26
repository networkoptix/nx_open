## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#--------------------------------------------------------------------------------------------------
# Define stub_analytics_plugin lib, dynamic, depends on nx_kit and nx_sdk.

set(STUB_ANALYTICS_PLUGIN_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
file(GLOB_RECURSE STUB_ANALYTICS_PLUGIN_SRC CONFIGURE_DEPENDS ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/*)

add_library(stub_analytics_plugin SHARED ${STUB_ANALYTICS_PLUGIN_SRC})
target_include_directories(stub_analytics_plugin PRIVATE ${STUB_ANALYTICS_PLUGIN_SRC_DIR})
target_link_libraries(stub_analytics_plugin PRIVATE nx_kit nx_sdk)

target_compile_definitions(stub_analytics_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
)

if(NOT WIN32)
    target_link_libraries(stub_analytics_plugin PRIVATE pthread)
endif()

#--------------------------------------------------------------------------------------------------
# Copy object_streamer files.

file(COPY "object_streamer_files/" DESTINATION "object_streamer/" FILES_MATCHING PATTERN "*")
