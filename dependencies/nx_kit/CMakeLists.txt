## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(WIN32 OR BUILD_SHARED_LIBS)
    set(nxKitLibraryType "SHARED")
else()
    set(nxKitLibraryType "STATIC")
endif()
set(nxKitLibraryType ${nxKitLibraryType} CACHE STRING "" FORCE)

set(nxKitWithTests ${withTests} CACHE STRING "" FORCE)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../artifacts/nx_kit
    ${CMAKE_BINARY_DIR}/open/artifacts/nx_kit)

if(withTests)
    add_dependencies(unit_tests nx_kit_ut)
endif()

set_target_properties(nx_kit PROPERTIES FOLDER sdk)
if(TARGET nx_kit_ut)
    set_target_properties(nx_kit_ut PROPERTIES FOLDER sdk)
    nx_set_test_meta_info(nx_kit_ut PROJECT NXLIB)
endif()

if(TARGET disabled_ini_config_ut)
    set_target_properties(disabled_ini_config_ut PROPERTIES FOLDER sdk)
endif()
