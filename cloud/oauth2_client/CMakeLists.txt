## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(oauth2_client
    LIBRARY NO_MOC
    PUBLIC_LIBS nx_network cloud_db_client
    FOLDER common/libs
)

target_compile_definitions(oauth2_client
        PRIVATE NX_OAUTH2_CLIENT_API=${API_EXPORT_MACRO}
        INTERFACE NX_OAUTH2_CLIENT_API=${API_IMPORT_MACRO})
