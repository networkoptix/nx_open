add_custom_target(cloud-docker-build-prepare)
add_custom_command(TARGET cloud-docker-build-prepare COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(vms_gateway)
add_subdirectory(connection_mediator)
add_subdirectory(cloud_db)
add_subdirectory(traffic_relay)
add_subdirectory(cloud_portal)

add_custom_target(cloud-docker-build DEPENDS vms_gateway-docker-build connection_mediator-docker-build cloud_db-docker-build traffic_relay-docker-build cloud_portal-docker-build)
add_custom_target(cloud-docker-push DEPENDS vms_gateway-docker-push connection_mediator-docker-push cloud_db-docker-push traffic_relay-docker-push cloud_portal-docker-push)
