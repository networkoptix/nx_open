nx_add_target(cloud_connectivity_ut EXECUTABLE PUBLIC_LIBS libconnection_mediator GTest GMock)
target_include_directories(cloud_connectivity_ut PRIVATE "${PROJECT_SOURCE_DIR}/nx_cloud")
