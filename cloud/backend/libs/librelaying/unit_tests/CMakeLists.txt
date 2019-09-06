nx_add_test(relaying_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_relaying)
