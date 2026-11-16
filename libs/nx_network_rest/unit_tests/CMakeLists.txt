## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(openapi)

set(testSourceDir "${CMAKE_CURRENT_SOURCE_DIR}/src/nx/network/rest")
set(versionedOpenapiJsons)
foreach(apiVersion RANGE 3 5)
    set(_openapiJson "${CMAKE_CURRENT_BINARY_DIR}/openapi_v${apiVersion}.json")
    list(APPEND versionedOpenapiJsons "${_openapiJson}")
    nx_generate_openapi_schema(
        TEMPLATE "${testSourceDir}/openapi_template_ut.yaml"
        VMS_PATH "${open_source_root}"
        CONFIG "${testSourceDir}/apidoctool_v${apiVersion}.properties"
        TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/openapi_tmp_v${apiVersion}"
        OUTPUT "${_openapiJson}"
        VERBOSE "-verbose"
    )
endforeach()

nx_add_test(nx_network_rest_ut
    ADDITIONAL_RESOURCES ${versionedOpenapiJsons}
    PUBLIC_LIBS
        nx_network_rest
        nx_vms_api # nx_network_rest itself does not depend on nx_vms_api. This dependency is
                   # needed to verify nx_vms_api structs against nx_network_rest.
    PROJECT NXLIB
    FOLDER common/tests
)
