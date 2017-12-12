if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_test(utils_ut PUBLIC_LIBS nx_utils Qt5::Network GTest GMock)
