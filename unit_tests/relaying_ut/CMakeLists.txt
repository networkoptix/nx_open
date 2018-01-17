if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_test(relaying_ut EXECUTABLE PUBLIC_LIBS nx_relaying GTest GMock)
