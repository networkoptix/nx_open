nx_add_test(nx_analytics_db_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_analytics_db)
