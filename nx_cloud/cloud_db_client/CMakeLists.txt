if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_target(cloud_db_client LIBRARY PUBLIC_LIBS nx_network)

target_include_directories(cloud_db_client PUBLIC src/include)
