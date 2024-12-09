## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(openapi)

if(withSdk)
    add_subdirectory(nx_server_plugin_sdk)
endif()

add_subdirectory(plugins)

set(versionedOpenapiJsons)
foreach(apiVersion RANGE 1 4)
    set(_openapiJson "${CMAKE_CURRENT_BINARY_DIR}/api/openapi_v${apiVersion}.json")
    list(APPEND versionedOpenapiJsons "${_openapiJson}")
    nx_generate_openapi_schema(
        TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/api/v${apiVersion}/openapi_template.yaml"
        VMS_PATH "${open_source_root}"
        CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/api/v${apiVersion}/apidoctool.properties"
        TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/api/openapi_v${apiVersion}_tmp"
        OUTPUT "${_openapiJson}"
        VERBOSE "-verbose"
    )
endforeach()

add_custom_target(generate_openapi_schemas_common DEPENDS ${versionedOpenapiJsons})

set_target_properties(generate_openapi_schemas_common PROPERTIES
    FOLDER common
    OUTPUT_FILES "${versionedOpenapiJsons}"
)
