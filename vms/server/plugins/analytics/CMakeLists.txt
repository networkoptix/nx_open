if(enable_hanwha)
    add_subdirectory(hanwha_analytics_plugin)
endif()

add_subdirectory(hikvision_analytics_plugin)

add_subdirectory(axis_analytics_plugin)

add_subdirectory(vca_analytics_plugin)

add_subdirectory(dw_mtt_analytics_plugin)

add_subdirectory(dw_tvt_analytics_plugin)

add_subdirectory(ssc_analytics_plugin)

add_subdirectory(dahua_analytics_plugin)

<<<<<<< HEAD
=======
add_subdirectory(bosch_analytics_plugin)

if(targetDevice STREQUAL "linux_arm64")
    add_subdirectory(deepstream_analytics_plugin)
endif()

>>>>>>> d20e3169329... VMS-19017: crate empty project
add_subdirectory(vivotek_analytics_plugin)

add_subdirectory(sample_analytics_plugin)

add_subdirectory(stub_analytics_plugin)
