nx_add_test(nx_cassandra_ut NO_MOC
    PUBLIC_LIBS
        GTest GMock
        nx_cassandra
        nx_utils
        libtraffic_relay
)
