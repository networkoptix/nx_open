set(product.title "${testcamera.name}")

nx_add_target(testcamera EXECUTABLE
    PUBLIC_LIBS
        common qtsingleapplication
)