if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_target(connection_mediator EXECUTABLE PUBLIC_LIBS libconnection_mediator)
