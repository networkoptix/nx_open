if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_test(vms_gateway_ut PUBLIC_LIBS vms_gateway_core mediaserver_core GTest GMock)
