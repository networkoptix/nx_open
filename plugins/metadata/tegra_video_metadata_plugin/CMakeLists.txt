nx_add_target(tegra_video_metadata_plugin LIBRARY NO_MOC NO_PCH
    PUBLIC_LIBS
        Qt5::Gui
        common nx_utils
    PRIVATE_LIBS
        tegra_video nx_kit
)
