## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_server_plugin(rpi_camera_plugin OPTIONAL NO_MOC NO_GTEST
    PRIVATE_LIBS
        nx_sdk
    ADD_TO_DISTRIBUTION_IF FALSE
)

find_package(rpi-firmware)
target_link_libraries(rpi_camera_plugin PRIVATE rpi-firmware::rpi-firmware)
