nx_add_test(nx_cloud_storage_client_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS nx_cloud_storage_client)
