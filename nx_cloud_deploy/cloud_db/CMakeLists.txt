add_custom_target(cloud_db-docker-build)

add_dependencies(cloud_db-docker-build cloud_db)
add_dependencies(cloud_db-docker-build cloud-docker-build-prepare)

set(cloud_db_location "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cloud_db")
add_custom_command(TARGET cloud_db-docker-build PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory stage/bin)
add_custom_command(TARGET cloud_db-docker-build PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory stage/qt/bin)
add_custom_command(TARGET cloud_db-docker-build COMMAND ${CMAKE_COMMAND} -E copy ${cloud_db_location} stage/bin/)
# copy cloud_db dependencies to lib/ qt/lib/ qt/bin/sqldrivers/

add_custom_target(cloud_db-docker-push DEPENDS cloud_db-docker-build COMMAND ./make.sh push)
