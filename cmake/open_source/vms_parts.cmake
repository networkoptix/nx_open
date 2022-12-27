## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This file is specific for open directory and must not be used in main project.

set(_withDistributions ON)
set(_withMiniLauncher ON)
set(_withSdk ON)
set(_withUnitTestsArchive ON)

if(developerBuild)
    set(_withDistributions OFF)
    set(_withMiniLauncher OFF)
    set(_withSdk OFF)
    set(_withUnitTestsArchive OFF)
endif()

# Windows distributions cannot be built without release libraries.
if(WINDOWS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_withDistributions OFF)
    set(_withUnitTestsArchive OFF)
endif()

option(withDesktopClient "Enable desktop client" ON)
option(withDistributions "Enable distributions build" ${_withDistributions})
option(withDocumentation "Generate documentation" ON)
option(withTests "Enable unit tests" ON)
option(withUnitTestsArchive "Enable unit tests archive generation" ${withUnitTestsArchive})

# Platform-specific options.
if(WINDOWS)
    option(withMiniLauncher "Enable minilauncher" ${_withMiniLauncher})
endif()

if(NOT MACOSX)
    option(withSdk "Enable building all SDKs" ${_withSdk})
endif()

unset(_withMiniLauncher)
unset(_withDistributions)
unset(_withSdk)
unset(_withUnitTestsArchive)
