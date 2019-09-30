nx_add_server_plugin(axis_camera_plugin OPTIONAL NO_GTEST
    PRIVATE_LIBS
        Qt5::Core
        Qt5::Network
        nx_sdk
    ADD_TO_DISTRIBUTION_IF FALSE
)
