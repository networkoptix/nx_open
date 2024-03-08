## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(CMAKE_SYSTEM_NAME Darwin)

execute_process(
    COMMAND sh -c "xcodebuild -version | awk '/Xcode/ {print $2}'"
    OUTPUT_VARIABLE xcode_version)

string(REGEX MATCH "([0-9]+)\\.([0-9]+)" has_xcode_version ${xcode_version})
if(has_xcode_version)
    set(major ${CMAKE_MATCH_1})
    set(minor ${CMAKE_MATCH_2})

    # We have to set deployment target to 13.3 while using XCode 15.3+ since it uses features from
    # libc++ which are only available in macOS 13.3+.
    if(major LESS_EQUAL 15 AND minor LESS 3)
        set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE INTERNAL "")
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET 13.3 CACHE INTERNAL "")
    endif()
else()
    message(FATAL_ERROR "Can't get XCode version, generation failed")
endif()
