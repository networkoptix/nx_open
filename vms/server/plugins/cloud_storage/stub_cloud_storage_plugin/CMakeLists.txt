## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_server_plugin(stub_cloud_storage_plugin NO_MOC OPTIONAL
        PRIVATE_LIBS
        nx_sdk
)

if(LINUX)
    target_link_libraries(stub_cloud_storage_plugin PRIVATE pthread)
endif()
