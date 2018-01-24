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
