nx_add_test(cloud_connect_integration_tests
    PUBLIC_LIBS
        libcloud_db
        libconnection_mediator
        libtraffic_relay)

target_include_directories(cloud_connect_integration_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/nx_cloud/backend"
)
