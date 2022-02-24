## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(nxSdkVersion "${releaseVersion} ${metaVersion}")
if(nxSdkVersion MATCHES "^ " OR nxSdkVersion MATCHES " $")
    message(FATAL_ERROR "INTERNAL ERROR: Either releaseVersion or metaVersion not set.")
endif()
if(developerBuild)
    string(APPEND nxSdkVersion " dev")
    if (NOT "${changeSet}" STREQUAL "")
        string(APPEND nxSdkVersion " ${changeSet}")
    endif()
endif()
