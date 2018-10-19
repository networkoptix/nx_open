nx_add_test(cloud_connectivity_ut
    NO_MOC
    NO_RC_FILE
    PUBLIC_LIBS
        libconnection_mediator
        GTest
        GMock
)
target_include_directories(cloud_connectivity_ut PRIVATE "${PROJECT_SOURCE_DIR}/nx_cloud/backend")
