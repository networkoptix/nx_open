## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(customCloudHost "" CACHE STRING
    "Cloud host. Leave empty to use cloud host from the customization package.")

function(set_cloud_host)
    if(NOT customCloudHost STREQUAL "")
        message(STATUS "Using the provided cloud host ${customCloudHost}")
        set(cloudHost ${customCloudHost})
    else()
        message(STATUS "Get cloud host from the customization package")
        set(cloudHost ${customization.cloudHost})
        message(STATUS "Using cloud host ${cloudHost}")
    endif()

    nx_expose_variables_to_parent_scope(cloudHost)
endfunction()

set_cloud_host()
