nx_add_test(cloud_db_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        libcloud_db
        nx_vms_server_db)
