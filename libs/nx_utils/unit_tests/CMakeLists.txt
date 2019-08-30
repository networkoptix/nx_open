nx_add_test(nx_utils_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_utils
        nx_fusion
        Qt5::Network)
