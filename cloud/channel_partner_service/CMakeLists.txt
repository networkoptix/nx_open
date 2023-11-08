## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(channel_partner_service
    LIBRARY LIBRARY_TYPE STATIC NO_MOC NO_PCH NO_API_MACROS
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS nx_network cloud_db_client
    FOLDER common/libs
)
