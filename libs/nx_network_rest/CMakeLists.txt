## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_network_rest LIBRARY
    WERROR
    PUBLIC_LIBS nx_json_rpc
    FOLDER common/libs
)

nx_make_target_translatable(nx_network_rest COMPONENTS "server")

target_compile_definitions(nx_network_rest
    PRIVATE NX_NETWORK_REST_API=${API_EXPORT_MACRO}
    INTERFACE NX_NETWORK_REST_API=${API_IMPORT_MACRO}
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
