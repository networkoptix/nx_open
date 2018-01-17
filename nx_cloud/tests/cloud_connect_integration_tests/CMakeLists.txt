if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_target(cloud_connect_integration_tests EXECUTABLE
    PUBLIC_LIBS
        GTest GMock
        libconnection_mediator vms_gateway_core libtraffic_relay
)

target_include_directories(cloud_connect_integration_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/nx_cloud"
)
