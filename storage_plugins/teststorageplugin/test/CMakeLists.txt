set_output_directories(RUNTIME "bin")

nx_add_target(
    teststorageplugin_ut 
        EXECUTABLE 
        NO_MOC NO_PCH 
        PUBLIC_LIBS GTest teststorageplugin)
