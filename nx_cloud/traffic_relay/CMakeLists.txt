if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_target(traffic_relay EXECUTABLE PUBLIC_LIBS libtraffic_relay)
