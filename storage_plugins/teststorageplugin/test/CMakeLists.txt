set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

nx_add_target(
    teststorageplugin_ut 
        EXECUTABLE 
        NO_MOC NO_PCH 
        PUBLIC_LIBS GTest teststorageplugin)
