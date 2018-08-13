add_definitions(-DENABLE_TEST_CAMERA)

nx_add_target(test_camera_core LIBRARY
    PUBLIC_LIBS common
)
