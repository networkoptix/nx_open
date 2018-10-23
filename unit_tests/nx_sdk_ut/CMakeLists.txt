nx_add_test(nx_sdk_ut
    WERROR
    NO_RC_FILE
    PUBLIC_LIBS
        GTest
        GMock
        nx_sdk
)
