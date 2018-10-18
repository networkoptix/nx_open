nx_add_test(utils_ut WERROR NO_MOC
    PUBLIC_LIBS
        nx_utils
        nx_fusion
        Qt5::Network
        GTest
        GMock)
