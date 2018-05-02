add_subdirectory(nx_kit)

if(NOT ANDROID AND NOT IOS)
    add_subdirectory(quazip)
endif()

add_subdirectory(tegra_video)

if(withMediaServer OR withClouds)
    add_subdirectory(qtservice)
endif()

if(withMediaServer OR withClouds OR withDesktopClient)
    add_subdirectory(qtsinglecoreapplication)
endif()

if(withDesktopClient)
    add_subdirectory(qtsingleapplication)
endif()

if(withMobileClient)
    add_subdirectory(qtsingleguiapplication)
endif()

if(withTests)
    add_subdirectory(googletest)
endif()
