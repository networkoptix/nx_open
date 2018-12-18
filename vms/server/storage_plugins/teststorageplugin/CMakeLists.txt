nx_add_target(teststorageplugin LIBRARY NO_MOC
    PRIVATE_LIBS
        nx_sdk
)

if(withTests)
    add_subdirectory(test)
endif()
