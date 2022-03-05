## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_subdirectory(concurrentqueue)
add_subdirectory(nx_kit)
add_subdirectory(udt)

if(withTests)
    add_subdirectory(googletest)
endif()

if(NOT ANDROID AND NOT IOS)
    add_subdirectory(quazip)
endif()

if(withDesktopClient)
    if(WINDOWS)
        add_subdirectory(detours)
    endif()

    add_subdirectory(qthttpserver)
endif()

if((withDesktopClient OR withMobileClient) AND NOT (ANDROID OR IOS))
    add_subdirectory(qtkeychain)
endif()

if(withMediaServer OR withClouds OR withDesktopClient)
    add_subdirectory(qtsinglecoreapplication)
endif()
