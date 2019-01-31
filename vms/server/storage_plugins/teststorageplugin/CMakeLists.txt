nx_add_target(teststorageplugin LIBRARY NO_MOC)

if(withTests)
    add_subdirectory(test)
endif()
