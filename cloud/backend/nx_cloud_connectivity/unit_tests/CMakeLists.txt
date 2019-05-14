nx_add_test(cloud_connectivity_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        libconnection_mediator)
target_include_directories(cloud_connectivity_ut PRIVATE "${PROJECT_SOURCE_DIR}/nx_cloud/backend")
