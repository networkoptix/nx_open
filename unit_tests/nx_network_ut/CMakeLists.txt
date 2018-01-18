if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_test(nx_network_ut
    PUBLIC_LIBS
        GTest GMock
        nx_network
)
