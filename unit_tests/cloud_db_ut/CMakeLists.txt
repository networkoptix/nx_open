if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_test(cloud_db_ut PUBLIC_LIBS libcloud_db GTest GMock)

