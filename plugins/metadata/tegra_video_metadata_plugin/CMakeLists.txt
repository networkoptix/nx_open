nx_add_server_plugin(tegra_video_metadata_plugin NO_MOC OPTIONAL
    PUBLIC_LIBS
        Qt5::Gui
        common nx_utils
    PRIVATE_LIBS
        tegra_video nx_kit
)
