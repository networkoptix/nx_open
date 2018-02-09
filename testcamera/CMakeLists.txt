set(product.title "${testcamera.name}")

add_definitions(-DENABLE_TEST_CAMERA)

nx_add_target(testcamera EXECUTABLE
    PUBLIC_LIBS
        common
)
