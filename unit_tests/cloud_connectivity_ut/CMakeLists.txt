if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_test(cloud_connectivity_ut PUBLIC_LIBS libconnection_mediator GTest GMock)
target_include_directories(cloud_connectivity_ut PRIVATE "${PROJECT_SOURCE_DIR}/nx_cloud")
