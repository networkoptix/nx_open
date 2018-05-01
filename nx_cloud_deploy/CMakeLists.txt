#-----------------------------------------------------------------------------------------------------------
# Utility functions

function(nx_add_cloud_deploy_target name)
    add_custom_target(${name}-docker-build)

    if(TARGET ${name})
        add_dependencies(${name}-docker-build ${name})
    endif()
    add_dependencies(${name}-docker-build cloud-docker-build-prepare)
    add_custom_command(TARGET ${name}-docker-build COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_SOURCE_DIR} ./make.sh stage_cmake ${CMAKE_BINARY_DIR} ${name})

    add_custom_command(TARGET ${name}-docker-build COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/stage/qt/bin/sqldrivers)
    add_custom_command(TARGET ${name}-docker-build COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIR}/plugins/sqldrivers/libqsqlmysql.so ${CMAKE_CURRENT_BINARY_DIR}/stage/qt/bin/sqldrivers/)

    add_custom_command(TARGET ${name}-docker-build COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_CURRENT_SOURCE_DIR} VERSION=${cloudVersion} ./make.sh pack)

    add_custom_target(${name}-docker-push DEPENDS ${name}-docker-build COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_SOURCE_DIR} VERSION=${cloudVersion} ./make.sh push)
endfunction(nx_add_cloud_deploy_target)

function(nx_fetch_cloud_build_number buildNumberVarName)
	set (get_build_number_cmd "curl -s -XPOST la.hdw.mx:8007/next/cloud/ | sed 's/{.*:\\([0-9]*\\)}/\\1/g'")
	execute_process(
    	COMMAND  bash -c ${get_build_number_cmd}
	    OUTPUT_VARIABLE buildNumber)

	if(NOT "${buildNumber}" MATCHES ^[0-9]+$)
    	message(FATAL_ERROR "Failed to get build number. Configuration is stopped")
	endif()

	set(${buildNumberVarName} ${buildNumber} PARENT_SCOPE)
endfunction(nx_fetch_cloud_build_number)

function(nx_get_cloud_version versionVarName)
	nx_fetch_cloud_build_number(buildNumber)
	
	file(READ ${CMAKE_SOURCE_DIR}/nx_cloud/version.txt cloudVersion)
	string(STRIP ${cloudVersion} cloudVersion)
	set (cloudVersion "${cloudVersion}.${buildNumber}")

	set(${versionVarName} ${cloudVersion} PARENT_SCOPE)
endfunction(nx_get_cloud_version)

function(define_cloud_backend_targets)
    set(cloudBackendModules cloud_db connection_mediator traffic_relay vms_gateway)

    add_custom_target(cloud-backend-docker-build)
    add_custom_target(cloud-backend-docker-push DEPENDS cloud-backend-docker-build)

    foreach(cloudModule ${cloudBackendModules})
        add_subdirectory(${cloudModule})
        add_dependencies(cloud-backend-docker-build ${cloudModule}-docker-build)
        add_dependencies(cloud-backend-docker-push ${cloudModule}-docker-push)
    endforeach(cloudModule)
endfunction(define_cloud_backend_targets)

function(define_cloud_frontend_targets)
    set(cloudFrontendModules cloud_portal cloud_portal_nginx nxcloud_host_agent)

    add_custom_target(cloud-frontend-docker-build)
    add_custom_target(cloud-frontend-docker-push DEPENDS cloud-frontend-docker-build)

    foreach(cloudModule ${cloudFrontendModules})
        add_subdirectory(${cloudModule})
        add_dependencies(cloud-frontend-docker-build ${cloudModule}-docker-build)
        add_dependencies(cloud-frontend-docker-push ${cloudModule}-docker-push)
    endforeach(cloudModule)
endfunction(define_cloud_frontend_targets)

#-----------------------------------------------------------------------------------------------------------

nx_get_cloud_version(cloudVersion)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cloud_full_version "${cloudVersion}")
message("-- Configuring cloud build ${cloudVersion}")

add_custom_target(cloud-docker-build-prepare)
add_custom_command(TARGET cloud-docker-build-prepare COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

define_cloud_backend_targets()
define_cloud_frontend_targets()

add_custom_target(cloud-docker-build DEPENDS cloud-backend-docker-build cloud-frontend-docker-build)
add_custom_target(cloud-docker-push DEPENDS cloud-docker-build cloud-backend-docker-push cloud-frontend-docker-push)
