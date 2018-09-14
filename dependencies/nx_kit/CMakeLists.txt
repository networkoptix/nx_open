if(WIN32)
    set(nxKitLibraryType "SHARED" CACHE STRING "" FORCE)
endif()

set(nxKitWithTests ${withTests} CACHE STRING "" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/open/artifacts/nx_kit
    ${CMAKE_BINARY_DIR}/open/artifacts/nx_kit)

if(withTests)
    add_dependencies(unit_tests nx_kit_ut)
endif()

