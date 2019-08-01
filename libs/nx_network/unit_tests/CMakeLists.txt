nx_add_test(nx_network_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_network)
