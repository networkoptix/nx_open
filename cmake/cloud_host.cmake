set(cloudGroup "test" CACHE STRING "Cloud instance group (dev, test, demo, prod)")
set(customCloudHost "" CACHE STRING "Cloud host. Leave empty to fetch cloud host from server.")

if(NOT build_utils_dir)
    set(build_utils_dir "${CMAKE_SOURCE_DIR}/build_utils")
endif()

function(get_cloud_host cloudGroup customization result_variable)
    set(var cloud_hosts.groups.${cloudGroup}.${customization})
    set(cloudHost ${${var}})
    if("${cloudHost}" STREQUAL "")
        message(FATAL_ERROR "Could not find cloud host for ${customization}:${cloudGroup}.")
    else()
        message(STATUS "Using cloud host ${cloudHost}")
        set(${result_variable} ${cloudHost} PARENT_SCOPE)
    endif()
endfunction()

function(set_cloud_hosts)
    set(cloud_hosts_cmake ${CMAKE_BINARY_DIR}/cloud_hosts.cmake)
    nx_json_to_cmake(
        ${PACKAGES_DIR}/any/cloud_hosts/cloud_hosts.json
        ${cloud_hosts_cmake}
         cloud_hosts)

    if(customCloudHost STREQUAL "")
        include(${cloud_hosts_cmake})
        get_cloud_host(${cloudGroup} ${customization} cloudHost)

        set(compatibleCloudHosts)
        foreach(customization ${customization.mobile.compatibleCustomizations})
            get_cloud_host(${cloudGroup} ${customization} host)
            list(APPEND compatibleCloudHosts ${host})
        endforeach()
    else()
        message(STATUS "Using the provided cloud host ${customCloudHost}")
    endif()

    nx_expose_variables_to_parent_scope(cloudHost compatibleCloudHosts)
endfunction()

set_cloud_hosts()
