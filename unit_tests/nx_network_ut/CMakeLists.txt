nx_add_target(nx_network_ut EXECUTABLE
    PUBLIC_LIBS
        GTest GMock
        nx_network
)

if(NOT WINDOWS)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()
