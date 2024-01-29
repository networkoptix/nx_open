## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(plugin_utils)

add_subdirectory(sample_analytics_plugin)
add_subdirectory(stub_analytics_plugin)
add_subdirectory(gpt4vision_analytics_plugin)

if(withTests)
    add_subdirectory(unit_tests)
endif()
