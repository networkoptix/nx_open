## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(openapi)

nx_generate_openapi_schema(
    TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/src/nx/network/rest/openapi_template_ut.yaml"
    VMS_PATH "${open_source_root}"
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/src/nx/network/rest/apidoctool_v4.properties"
    TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/openapi_tmp_v4"
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/openapi_v4.json"
    VERBOSE "-verbose")

nx_generate_openapi_schema(
    TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/src/nx/network/rest/openapi_template_ut.yaml"
    VMS_PATH "${open_source_root}"
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/src/nx/network/rest/apidoctool_v3.properties"
    TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/openapi_tmp_v3"
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/openapi_v3.json"
    VERBOSE "-verbose")

nx_add_test(nx_network_rest_ut
    ADDITIONAL_RESOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/openapi_v4.json
        ${CMAKE_CURRENT_BINARY_DIR}/openapi_v3.json
    PUBLIC_LIBS
        nx_network_rest
        nx_vms_api # nx_network_rest itself does not depend on nx_vms_api. This dependency is
                   # needed to verify nx_vms_api structs against nx_network_rest.
    PROJECT NXLIB
    FOLDER common/tests
)
