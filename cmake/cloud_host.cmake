set(cloudGroup "test" CACHE STRING "Cloud instance group (dev, test, demo, prod)")
set(customCloudHost "" CACHE STRING "Cloud host. Leave empty to fetch cloud host from server.")

function(get_cloud_host cloudGroup customization result_variable)
    set(var cloud_hosts.groups.${cloudGroup}.${customization})
    set(cloudHost ${${var}})
    if("${cloudHost}" STREQUAL "")
        message(FATAL_ERROR "Could not find cloud host for ${customization}:${cloudGroup}.")
    else()
        message("Using cloud host ${cloudHost}")
        set(${result_variable} ${cloudHost} PARENT_SCOPE)
    endif()
endfunction()

function(set_cloud_hosts)
    set(cloud_hosts_json ${PACKAGES_DIR}/any/cloud_hosts/cloud_hosts.json)
    set(cloud_hosts_cmake ${CMAKE_BINARY_DIR}/cloud_hosts.cmake)
    if(${cloud_hosts_json} IS_NEWER_THAN ${cloud_hosts_cmake})
        message(${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build_utils/json_to_cmake.py)
        execute_process(COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/build_utils/json_to_cmake.py
            ${cloud_hosts_json} ${cloud_hosts_cmake} -p cloud_hosts)
    endif()

    if(customCloudHost STREQUAL "")
        include(${cloud_hosts_cmake})
        get_cloud_host(${cloudGroup} ${customization} cloudHost)

        set(compatibleCloudHosts)
        foreach(customization ${compatibleCustomizations})
            get_cloud_host(${cloudGroup} ${customization} cloudHost)
            list(APPEND compatibleCloudHosts ${cloudHost})
        endforeach()
    else()
        message(STATUS "Using the provided cloud host ${customCloudHost}")
    endif()

    nx_expose_variables_to_parent_scope(cloudHost compatibleCloudHosts)
endfunction()

set_cloud_hosts()
