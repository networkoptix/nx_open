if(WIN32 OR BUILD_SHARED_LIBS)
    set(nxKitLibraryType "SHARED")
else()
    set(nxKitLibraryType "STATIC")
endif()
set(nxKitLibraryType ${nxKitLibraryType} CACHE STRING "" FORCE)

set(nxKitWithTests ${withTests} CACHE STRING "" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/open/artifacts/nx_kit
    ${CMAKE_BINARY_DIR}/open/artifacts/nx_kit)

if(withTests)
    add_dependencies(unit_tests nx_kit_ut)
endif()

