nx_add_server_plugin(rpi_camera_plugin OPTIONAL NO_MOC NO_GTEST
    PRIVATE_LIBS
        nx_sdk
    ADD_TO_DISTRIBUTION_IF FALSE
)

target_link_libraries(rpi_camera_plugin PRIVATE
    bcm_host
    openmaxil
)
