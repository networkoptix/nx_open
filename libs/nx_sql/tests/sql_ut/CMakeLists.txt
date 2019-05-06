nx_add_test(sql_ut NO_MOC WERROR
    PUBLIC_LIBS nx_sql GTest GMock)

target_compile_definitions(sql_ut PRIVATE kModuleName="sql_ut")
