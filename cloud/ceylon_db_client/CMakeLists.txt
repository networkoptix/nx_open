## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(ceylon_db_client
    LIBRARY NO_MOC
    PUBLIC_LIBS nx_network cloud_db_client
    FOLDER common/libs
)

target_compile_definitions(ceylon_db_client
    PRIVATE NX_CEYLONDB_CLIENT_API=${API_EXPORT_MACRO}
    INTERFACE NX_CEYLONDB_CLIENT_API=${API_IMPORT_MACRO})
