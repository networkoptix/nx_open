## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This file is specific for open directory and must not be used in main project.

include(CMakeDependentOption)

set(_withDesktopClient ON)
set(_withTests ON)
set(_withMiniLauncher OFF)

if(WINDOWS)
    if(NOT developerBuild)
        set(_withMiniLauncher ON)
    endif()
endif()

option(withDesktopClient "Enable desktop client" ${_withDesktopClient})
option(withTests "Enable unit tests" ${_withTests})
cmake_dependent_option(withMiniLauncher "Enable minilauncher"
    ${_withMiniLauncher} "NOT withDistributions"
    ON
)

option(withDocumentation "Generate documentation" ON)

cmake_dependent_option(withDistributions "Enable distributions build"
    OFF "developerBuild"
    ON
)

unset(_withDesktopClient)
unset(_withTests)
unset(_withMiniLauncher)
