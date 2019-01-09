if(enable_hanwha)
    add_subdirectory(hanwha_analytics_plugin)
endif()

add_subdirectory(tegra_video_analytics_plugin)

add_subdirectory(hikvision_analytics_plugin)

add_subdirectory(axis_analytics_plugin)

add_subdirectory(vca_analytics_plugin)

add_subdirectory(dw_mtt_analytics_plugin)

add_subdirectory(ssc_analytics_plugin)

add_subdirectory(dahua_analytics_plugin)

if(targetDevice STREQUAL "tx1")
    add_subdirectory(deepstream_analytics_plugin)
endif()

add_subdirectory(stub_analytics_plugin)
