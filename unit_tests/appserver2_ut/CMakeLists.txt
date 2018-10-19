nx_add_test(appserver2_ut
    NO_RC_FILE
    PUBLIC_LIBS
        appserver2
        mediaserver_db
        GTest
        GMock)
