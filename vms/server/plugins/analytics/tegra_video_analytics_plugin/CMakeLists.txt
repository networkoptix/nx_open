nx_add_server_plugin(tegra_video_analytics_plugin NO_MOC OPTIONAL
    PRIVATE_LIBS
        Qt5::Gui
        tegra_video nx_kit nx_plugin_utils
    ADD_TO_DISTRIBUTION_IF targetDevice STREQUAL "tx1"
)
