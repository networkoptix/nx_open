nx_add_test(sql_ut NO_MOC WERROR
    PUBLIC_LIBS nx_sql GTest GMock)
nx_force_include(sql_ut ${CMAKE_SOURCE_DIR}/libs/nx_utils/src/nx/utils/literal.h)
