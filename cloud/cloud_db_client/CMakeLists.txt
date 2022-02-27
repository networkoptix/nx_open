## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(cloud_db_client
    LIBRARY NO_MOC NO_API_MACROS
    PUBLIC_LIBS nx_network
    FOLDER common/libs
)

target_include_directories(cloud_db_client PUBLIC src/include)
