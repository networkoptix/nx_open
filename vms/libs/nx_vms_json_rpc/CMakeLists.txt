## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_vms_json_rpc LIBRARY NO_MOC
    PUBLIC_LIBS nx_fusion nx_network
    FOLDER common/libs
)

target_compile_definitions(nx_vms_json_rpc
    PRIVATE NX_VMS_JSON_RPC_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_JSON_RPC_API=${API_IMPORT_MACRO}
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
