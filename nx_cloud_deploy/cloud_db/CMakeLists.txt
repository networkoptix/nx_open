add_custom_target(cloud_db-docker-build)

add_dependencies(cloud_db-docker-build cloud_db)
add_dependencies(cloud_db-docker-build cloud-docker-build-prepare)
add_custom_command(TARGET cloud_db-docker-build COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_CURRENT_SOURCE_DIR} ./make.sh stage_cmake ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET cloud_db-docker-build COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_CURRENT_SOURCE_DIR} ./make.sh pack)

add_custom_target(cloud_db-docker-push DEPENDS cloud_db-docker-build COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_CURRENT_SOURCE_DIR} ./make.sh push)
