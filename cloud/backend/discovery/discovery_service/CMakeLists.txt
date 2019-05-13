add_custom_target(discovery_service)

function (nx_go_get)
    foreach (packageName ${ARGV})
        add_custom_command(
            TARGET discovery_service
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND ${CMAKE_COMMAND} -E env GOOS=linux go get ${packageName})
    endforeach (packageName)
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
    COMMAND ${CMAKE_COMMAND} -E env zip ./discovery_service-${cloudVersion}.zip ./discovery_service)
