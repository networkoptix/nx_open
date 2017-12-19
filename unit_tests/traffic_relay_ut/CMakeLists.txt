if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_test(traffic_relay_ut EXECUTABLE PUBLIC_LIBS libtraffic_relay nx_network GTest GMock)
