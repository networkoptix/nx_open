## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This file is specific to the "open/" directory and must not be used in the main project.

set(_withDistributions ON)
set(_withDocumentation ON)
set(_withMiniLauncher ON)
set(_withSdk ON)
set(_withUnitTestsArchive ON)

if(developerBuild)
    set(_withDistributions OFF)
    set(_withDocumentation OFF)
    set(_withMiniLauncher OFF)
    set(_withSdk OFF)
    set(_withUnitTestsArchive OFF)
endif()

# Windows distributions cannot be built without release libraries.
if(WINDOWS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_withDistributions OFF)
    set(_withUnitTestsArchive OFF)
endif()

option(withDesktopClient "Enable Desktop Client" ON)
option(withDistributions "Enable distributions" ${_withDistributions})
option(withDocumentation "Generate documentation" ${_withDocumentation})
option(withTests "Enable unit tests" ON)
option(withUnitTestsArchive "Enable unit tests archive" ${_withUnitTestsArchive})
set(withRootTool "false") #< Required in the build_info.json.

# Platform-specific options.
if(WINDOWS)
    option(withMiniLauncher "Enable minilauncher" ${_withMiniLauncher})
endif()

if(NOT MACOSX)
    option(withSdk "Enable building all SDKs" ${_withSdk})
endif()

unset(_withMiniLauncher)
unset(_withDistributions)
unset(_withDocumentation)
unset(_withSdk)
unset(_withUnitTestsArchive)

if(withUnitTestsArchive AND NOT withTests)
    message(WARNING "-DwithTests is OFF. Accordingly, switching OFF -DwithUnitTestsArchive option.")
    set(withUnitTestsArchive OFF)
endif()
