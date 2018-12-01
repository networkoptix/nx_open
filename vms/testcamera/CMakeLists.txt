set(file_description "${testcamera.name}")
set(file_internal_name "${testcamera.name}")

add_definitions(-DENABLE_TEST_CAMERA)

nx_add_target(testcamera EXECUTABLE
    PUBLIC_LIBS
        common
)

set_property(TARGET testcamera PROPERTY WIN32_EXECUTABLE OFF)
