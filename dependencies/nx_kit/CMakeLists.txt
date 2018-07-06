if(WIN32)
    set(nxKitLibraryType "SHARED" CACHE STRING "" FORCE)
endif()
include(${CMAKE_SOURCE_DIR}/open/artifacts/nx_kit/nx_kit.cmake)
