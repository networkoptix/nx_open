nx_add_test(connection_mediator_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        libconnection_mediator)
