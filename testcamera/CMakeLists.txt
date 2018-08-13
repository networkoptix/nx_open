set(file_description "${testcamera.name}")
set(file_internal_name "${testcamera.name}")

string(REPLACE "/SUBSYSTEM:WINDOWS /entry:mainCRTStartup" "" CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")

nx_add_target(test_camera EXECUTABLE
    PUBLIC_LIBS
        Qt5::Core
        test_camera_core
)
