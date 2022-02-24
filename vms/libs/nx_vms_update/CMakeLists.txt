## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_vms_update LIBRARY
    WERROR
    PUBLIC_LIBS
        nx_utils
        nx_fusion
        nx_network
        nx_vms_api
        nx_zip
    FOLDER common/libs
)

target_compile_definitions(nx_vms_update
    PRIVATE NX_VMS_UPDATE_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_UPDATE_API=${API_IMPORT_MACRO})
