add_subdirectory(nx_kit)

if(NOT ANDROID AND NOT IOS)
    add_subdirectory(quazip)
endif()

if(withMediaServer)
    add_subdirectory(tegra_video)
endif()

if(withMediaServer OR withClouds)
    add_subdirectory(qtservice)
endif()

if(withMediaServer OR withClouds OR withDesktopClient)
    add_subdirectory(qtsinglecoreapplication)
endif()

if(withDesktopClient)
    add_subdirectory(qtsingleapplication)
endif()

if((withDesktopClient OR withMobileClient) AND NOT (ANDROID OR IOS))
    add_subdirectory(qtkeychain)
endif()

if(withMobileClient)
    add_subdirectory(qtsingleguiapplication)
endif()

if(withTests)
    add_subdirectory(googletest)
endif()
