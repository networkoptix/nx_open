## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_vms_applauncher_api LIBRARY
    PUBLIC_LIBS
        nx_utils
    FOLDER client/libs
)

target_compile_definitions(nx_vms_applauncher_api
    PRIVATE NX_VMS_APPLAUNCHER_API_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_APPLAUNCHER_API_API=${API_IMPORT_MACRO})
