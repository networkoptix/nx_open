set(file_description "${testcamera.name}")
set(file_internal_name "${testcamera.name}")

add_definitions(-DENABLE_TEST_CAMERA)

string(REPLACE "/SUBSYSTEM:WINDOWS /entry:mainCRTStartup" "" CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")

nx_add_target(testcamera EXECUTABLE
    PUBLIC_LIBS
        common
)
