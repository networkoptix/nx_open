nx_add_target(common_ut EXECUTABLE
    PUBLIC_LIBS
        GTest GMock
        common
        nx_streaming #< Dependency of common.
)
