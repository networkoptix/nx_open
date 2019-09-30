if(targetDevice STREQUAL "linux_arm64")
    set(plugin_dir ${CMAKE_BINARY_DIR}/bin/plugins_optional/tegra_video_analytics_plugin)
    file(GLOB jetpack_files ${jetpack_directory}/lib/*)
    list(APPEND tegra_video_files
        ${tegra_video_directory}/bin/video_dec_gie
        ${tegra_video_directory}/lib/libtegra_video.so
    )
    add_custom_target(copy_tegra_video_files DEPENDS
        ${tegra_video_files}
        ${jetpack_files}
    )
    add_custom_command(
        TARGET copy_tegra_video_files
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${tegra_video_files} ${plugin_dir}/
        COMMAND ${CMAKE_COMMAND} -E copy ${jetpack_files} ${plugin_dir}/
        COMMENT "Installing tegra_video into tegra_video_analytics_plugin dedicated directory"
    )
endif()

nx_add_server_plugin(tegra_video_analytics_plugin NO_MOC OPTIONAL DEDICATED_DIR
    PRIVATE_LIBS
        Qt5::Gui #< for QVector2D
        boost #< for optional
        nx_sdk
        tegra_video
    ADD_TO_DISTRIBUTION_IF targetDevice STREQUAL "linux_arm64"
)

if(targetDevice STREQUAL "linux_arm64")
    add_dependencies(tegra_video_analytics_plugin copy_tegra_video_files)
endif()
