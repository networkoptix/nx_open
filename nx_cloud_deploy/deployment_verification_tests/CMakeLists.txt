# set(get_build_number_cmd "curl -s -XPOST la.hdw.mx:8007/next/cloud/ | sed 's/{.*:\\([0-9]*\\)}/\\1/g'")
# docker run -e USERID=$(id -u) -e GROUPID=$(id -g) --rm -ti -v $PWD:/go golang:1.10.2-alpine3.7 sh -c 'go build -o tests ./src; chown $USERID:$GROUPID tests'

add_custom_target(deployment_verification_tests)
add_custom_command(
    TARGET deployment_verification_tests
    COMMAND ${CMAKE_COMMAND} -E env docker run -e USERID=$(id -u) -e GROUPID=$(id -g) --rm -ti -v $PWD:/go golang:1.10.2-alpine3.7 sh -c 'go build -o deployment_verification_tests ./src; chown $USERID:$GROUPID tests')

#add_custom_command(
#    TARGET deployment_verification_tests
#    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
#    COMMAND ${CMAKE_COMMAND} -E env go build -o ${CMAKE_CURRENT_BINARY_DIR}/deployment_verification_tests .)

nx_add_cloud_deploy_target(deployment_verification_tests)
