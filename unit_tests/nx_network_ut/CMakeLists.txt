add_compile_options("-Werror" "-Wall" "-Wextra")

nx_add_target(nx_network_ut EXECUTABLE
    PUBLIC_LIBS
        GTest GMock
        nx_network common
        nx_streaming #< Dependency of common.
)
