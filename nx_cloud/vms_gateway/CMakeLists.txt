if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_target(vms_gateway EXECUTABLE PUBLIC_LIBS libvms_gateway_core nx_network nx_utils)
