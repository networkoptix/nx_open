nx_add_target(teststorageplugin LIBRARY NO_MOC NO_PCH)

if(withTests)
    add_subdirectory(test)
endif()
