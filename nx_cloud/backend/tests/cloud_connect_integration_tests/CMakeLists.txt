nx_add_test(cloud_connect_integration_tests EXECUTABLE NO_MOC
    PUBLIC_LIBS
        GTest GMock
        libconnection_mediator vms_gateway_core libtraffic_relay
)

target_include_directories(cloud_connect_integration_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/nx_cloud/backend"
)
