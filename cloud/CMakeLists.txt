function(nx_fetch_cloud_build_number buildNumberVarName)
    set(get_build_number_cmd "curl -s -XPOST la.hdw.mx:8007/next/cloud/ | sed 's/{.*:\\([0-9]*\\)}/\\1/g'")
    execute_process(
        COMMAND bash -c ${get_build_number_cmd}
            OUTPUT_VARIABLE buildNumber)

    if(NOT "${buildNumber}" MATCHES ^[0-9]+$)
        message(FATAL_ERROR "Failed to get build number. Configuration is stopped")
    endif()

    set(${buildNumberVarName} ${buildNumber} PARENT_SCOPE)
endfunction(nx_fetch_cloud_build_number)

function(nx_get_cloud_version versionVarName)
    if("${buildNumber}" STREQUAL "auto")
        nx_fetch_cloud_build_number(buildNumber)
    elseif(NOT buildNumber OR buildNumber EQUAL 0)
        message(STATUS "Using build number zero. You can specify the build number with -DbuildNumber=ddd. Or you can use -DbuildNumber=auto to generate the next build number")
    else()
        message(STATUS "Build number ${buildNumber} has been provided")
    endif()

    file(READ ${CMAKE_SOURCE_DIR}/cloud/version.txt cloudVersion)
    string(STRIP ${cloudVersion} cloudVersion)

    set(${versionVarName} "${cloudVersion}.${buildNumber}" PARENT_SCOPE)
endfunction(nx_get_cloud_version)

#--------------------------------------------------------------------------------------------------

if (withClouds)
    nx_get_cloud_version(cloudVersion)
endif()

add_subdirectory(backend)

if (withClouds AND LINUX)
    add_subdirectory(deploy)
endif()
