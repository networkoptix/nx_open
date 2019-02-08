#-----------------------------------------------------------------------------------------------------------
# Utility functions

function(nx_add_cloud_deploy_target name)
    add_custom_target(${name}-docker-build)

    if(TARGET ${name})
        add_dependencies(${name}-docker-build ${name})
    endif()
    add_dependencies(${name}-docker-build cloud-docker-build-prepare)
    add_custom_command(
        TARGET ${name}-docker-build
        COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_SOURCE_DIR} ./make.sh stage_cmake ${CMAKE_BINARY_DIR} ${name})

    add_custom_command(
        TARGET ${name}-docker-build
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/stage/qt/bin/sqldrivers)
    add_custom_command(
        TARGET ${name}-docker-build
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIR}/plugins/sqldrivers/libqsqlmysql.so ${CMAKE_CURRENT_BINARY_DIR}/stage/qt/bin/sqldrivers/)

    add_custom_command(
        TARGET ${name}-docker-build
        COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_CURRENT_SOURCE_DIR} VERSION=${cloudVersion} ./make.sh pack)

    add_custom_target(
        ${name}-docker-push DEPENDS ${name}-docker-build
        COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_SOURCE_DIR} VERSION=${cloudVersion} ./make.sh push)

    add_custom_target(
        ${name}-docker-clean
        COMMAND docker images -q ${name}:${cloudVersion} | xargs --no-run-if-empty docker rmi -f)
endfunction(nx_add_cloud_deploy_target)

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

function(nx_define_cloud_group_targets groupName)
    add_custom_target(cloud-${groupName}-docker-build)
    add_custom_target(cloud-${groupName}-docker-push DEPENDS cloud-${groupName}-docker-build)
    add_custom_target(cloud-${groupName}-docker-clean)

    foreach(cloudModule ${ARGN})
        add_subdirectory(${cloudModule})
        add_dependencies(cloud-${groupName}-docker-build ${cloudModule}-docker-build)
        add_dependencies(cloud-${groupName}-docker-push ${cloudModule}-docker-push)
        add_dependencies(cloud-${groupName}-docker-clean ${cloudModule}-docker-clean)
    endforeach(cloudModule)
endfunction(nx_define_cloud_group_targets)

#-----------------------------------------------------------------------------------------------------------

nx_get_cloud_version(cloudVersion)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cloud_full_version "${cloudVersion}")
nx_store_known_file(${CMAKE_CURRENT_BINARY_DIR}/cloud_full_version)
message(STATUS "Configuring cloud build ${cloudVersion}")

add_custom_target(cloud-docker-build-prepare)
add_custom_command(
    TARGET cloud-docker-build-prepare
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(cloudBackendModules cloud_db connection_mediator traffic_relay)
set(cloudFrontendModules cloud_portal cloud_portal_nginx nxcloud_host_agent)

nx_define_cloud_group_targets(backend ${cloudBackendModules})
nx_define_cloud_group_targets(frontend ${cloudFrontendModules})
add_subdirectory(deployment_verification_tests)

add_custom_target(
    cloud-docker-build 
    DEPENDS cloud-backend-docker-build 
        cloud-frontend-docker-build
        deployment_verification_tests-docker-build)

add_custom_target(
    cloud-docker-push
    DEPENDS cloud-docker-build 
        cloud-backend-docker-push 
        cloud-frontend-docker-push 
        deployment_verification_tests-docker-push)

add_custom_target(
    cloud-docker-clean
    DEPENDS
        cloud-backend-docker-clean 
        cloud-frontend-docker-clean 
        deployment_verification_tests-docker-clean)

add_subdirectory(cloud_connect_test_util)
