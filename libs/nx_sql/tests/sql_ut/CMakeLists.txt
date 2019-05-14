nx_add_test(sql_ut NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS nx_sql GTest GMock)
