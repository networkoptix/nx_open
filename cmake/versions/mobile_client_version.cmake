## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(mobileClientVersion 25.3.0)

# This variable is for testing purposes. Sometimes we need a custom build of an already published
# version (e.g. with another cloud instance). To be able to push it to TestFlight, we need it to be
# a not published version. This parameter allows us to override the version to any desired value.
set(customMobileClientVersion "" CACHE STRING "Custom Mobile Client version for testing purposes.")
if(customMobileClientVersion)
    set(mobileClientVersion ${customMobileClientVersion})
endif()

set(mobileClientVersion.full ${mobileClientVersion}.${buildNumber})
