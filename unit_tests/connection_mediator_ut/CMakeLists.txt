if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_test(connection_mediator_ut
    PUBLIC_LIBS libconnection_mediator libcloud_db GTest GMock)
