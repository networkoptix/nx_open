add_custom_target(deployment_verification_tests)
add_custom_command(
    TARGET deployment_verification_tests
    COMMAND ${CMAKE_COMMAND} -E env NX_VMS_DIR=${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/make.sh build ${CMAKE_CURRENT_SOURCE_DIR})

nx_add_cloud_deploy_target(deployment_verification_tests)
