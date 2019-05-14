nx_add_test(nx_clusterdb_map_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_clusterdb_map)
