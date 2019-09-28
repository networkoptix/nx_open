nx_add_test(traffic_relay_ut NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        libtraffic_relay
        nx_network)
