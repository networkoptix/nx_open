nx_add_server_plugin(tegra_video_analytics_plugin NO_MOC OPTIONAL
    PRIVATE_LIBS
        Qt5::Gui #< for QVector2D
        boost #< for optional
        nx_sdk
        tegra_video
    ADD_TO_DISTRIBUTION_IF targetDevice STREQUAL "tx1"
)
