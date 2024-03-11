## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(CMAKE_SYSTEM_NAME Darwin)

execute_process(
    COMMAND sh -c "clang++ --version | awk -F '(clang-|\\\\))' '{print $2}'"
    OUTPUT_VARIABLE clang_version)

string(REGEX MATCH "([0-9]+)\\.([0-9]+)" has_clang_version ${clang_version})

if(has_clang_version)
    set(major ${CMAKE_MATCH_1})
    set(minor ${CMAKE_MATCH_2})

    # We have to set deployment target to 13.3 while using clang from Xcode 15.3+ since it
    # uses features from libc++ which are only available in macOS 13.3+.
    if((major LESS 1500) OR (major EQUAL 1500 AND minor LESS 3))
        set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE INTERNAL "")
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET 13.3 CACHE INTERNAL "")
    endif()
else()
    message(FATAL_ERROR "Can't get XCode version, generation failed")
endif()
