nx_rdep_add_package(any/qtsingleguiapplication)
nx_rdep_add_package(any/roboto-ttf)
nx_rdep_add_package(any/roboto-mono-ttf)

add_subdirectory(libclient_core)

if(withDesktopClient)
    find_package(Qt5
        COMPONENTS
            Widgets
            QuickWidgets
            WebKit
            WebKitWidgets)

    if(LINUX)
        find_package(Qt5 COMPONENTS X11Extras)
    endif()

    add_subdirectory(nx_client_desktop)
    add_subdirectory(desktop_client)
    add_subdirectory(applauncher)
endif()

if(withMobileClient)
    add_subdirectory(mobile_client)
endif()
