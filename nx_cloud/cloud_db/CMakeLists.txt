if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_target(cloud_db EXECUTABLE PUBLIC_LIBS libcloud_db)
