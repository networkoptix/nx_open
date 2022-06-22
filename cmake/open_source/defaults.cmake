## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This file is specific for open directory and must not be used in main project.

# Turn on openSourceBuild flag.
set(openSourceBuild ON CACHE BOOL "Is open source build" FORCE)

# Values for cache variables not used in open source libraries but needed by cmake scripts common
# with the main project.
set(productType "vms" CACHE STRING "Product type" FORCE)
set(publicationType "local" CACHE STRING "Product publication type" FORCE)
set(buildNumber "0" CACHE STRING "Build number" FORCE)

set(withDesktopClient ON CACHE STRING "Enable desktop client" FORCE)

# Values for variables not used in open source libraries but needed by cmake scripts common with
# the main project.
set(isEdgeServer "false")
set(box "none")

set(statisticsServerUrl "" CACHE STRING "Default statistics server URL")
set(statisticsServerUser "" CACHE STRING "Default statistics server user")
set(statisticsServerPassword "" CACHE STRING "Default statistics server password")

if(WINDOWS)
    set(codeSigning false CACHE STRING "Sign created executables")
    set(trustedTimestamping false CACHE STRING "Use trusted timestamping for code signing")
endif()
