if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_target(cloud_connect_test_util EXECUTABLE PUBLIC_LIBS nx_network nx_utils)
