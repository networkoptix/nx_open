add_custom_target(discovery_service)

function (nx_go_get)
    add_custom_command(
        TARGET discovery_service
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} -E env GOOS=linux go get ${ARGV})
endfunction(nx_go_get)

nx_go_get (
    "github.com/akrylysov/algnhsa"
    "github.com/aws/aws-sdk-go/aws"
    "github.com/aws/aws-sdk-go/aws/session"
    "github.com/aws/aws-sdk-go/service/dynamodb"
    "github.com/julienschmidt/httprouter")

add_custom_command(
    TARGET discovery_service
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E env GOOS=linux go build -o discovery_service ./nx/cloud/discovery)

add_custom_command(
    TARGET discovery_service
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/discovery_service ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
    TARGET discovery_service
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_SOURCE_DIR}/discovery_service)

set (DISCOVERY_SERVICE_ARCHIVE_NAME "discovery_service-v${cloudVersion}.zip")

add_custom_command(
    TARGET discovery_service
    COMMAND ${CMAKE_COMMAND} -E env zip ./${DISCOVERY_SERVICE_ARCHIVE_NAME} ./discovery_service)

#--------------------------------------------------------------------------------------------------

add_custom_target(discovery_service-push)
add_dependencies(discovery_service-push discovery_service)

add_custom_command(
    TARGET discovery_service-push
    COMMAND ${CMAKE_COMMAND} -E env aws s3 cp ${DISCOVERY_SERVICE_ARCHIVE_NAME} s3://nx-lambda-packages/discovery_service/${DISCOVERY_SERVICE_ARCHIVE_NAME})
