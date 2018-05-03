nx_add_test(nx_data_sync_engine_ut WERROR
    PUBLIC_LIBS
        GTest GMock
        libcloud_db nx_data_sync_engine
)
