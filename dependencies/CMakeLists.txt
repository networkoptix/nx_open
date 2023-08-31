## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_subdirectory(concurrentqueue)
add_subdirectory(nx_kit)
add_subdirectory(udt)
add_subdirectory(quazip)
add_subdirectory(qt-mustache)
add_subdirectory(stun_oauth_token_tools)

if(withTests)
    add_subdirectory(googletest)
endif()

if(withDesktopClient)
    add_subdirectory(qthttpserver)
endif()

if(withDesktopClient OR withMobileClient)
    add_subdirectory(speex)
endif()

if((withDesktopClient OR withMobileClient) AND NOT (ANDROID OR IOS))
    add_subdirectory(qtkeychain)
endif()

if(withMediaServer OR withClouds OR withDesktopClient)
    add_subdirectory(qtsinglecoreapplication)
endif()

