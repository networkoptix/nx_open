## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This file is specific to the "open/" directory and must not be used in the main project.

# Turn on openSourceBuild flag.
set(openSourceBuild ON CACHE BOOL "Is open source build" FORCE)

# Values for cache variables not used in open source libraries but needed by cmake scripts common
# with the main project.
set(productType "vms" CACHE STRING "Product type" FORCE)
set(publicationType "local" CACHE STRING "Product publication type")
set(buildNumber "0" CACHE STRING "Build number")

set(withDesktopClient ON CACHE STRING "Enable desktop client")

set(statisticsServerUrl "" CACHE STRING "Default statistics server URL")
set(statisticsServerUser "" CACHE STRING "Default statistics server user")
set(statisticsServerPassword "" CACHE STRING "Default statistics server password")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(signtoolConfig "" CACHE PATH "Path to the configuration file of the signing tool. Leave empty to turn off the signing.")

    if(signtoolConfig)
        set(codeSigning true)
    else()
        set(codeSigning false)
    endif()
endif()
