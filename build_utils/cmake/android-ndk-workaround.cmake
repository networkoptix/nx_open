if(ANDROID)
    # This lines are temporary copied from cmake 3.8. Remove them once cmake 3.8 is released.
    list(APPEND CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${CMAKE_SYSROOT}/usr/include")
endif()
