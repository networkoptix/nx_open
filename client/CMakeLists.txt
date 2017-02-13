rdep_add_package(any/qtsingleguiapplication)
rdep_add_package(any/roboto-ttf)
rdep_add_package(any/roboto-mono-ttf)

add_subdirectory(libclient_core)

if(withDesktopClient)
    add_subdirectory(libclient)
    add_subdirectory(desktop_client)
    add_subdirectory(applauncher)
endif()

if(withMobileClient)
    add_subdirectory(mobile_client)
endif()
