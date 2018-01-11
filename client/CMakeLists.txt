add_subdirectory(nx_client_core)

if(withDesktopClient)
    find_package(Qt5
        COMPONENTS
            OpenGL
            QuickWidgets
            WebKit
            WebKitWidgets)

    if(LINUX)
        find_package(Qt5 COMPONENTS X11Extras)
    endif()

    if(WINDOWS)
        add_subdirectory(minilauncher)
    endif()

    add_subdirectory(nx_client_desktop)
    add_subdirectory(desktop_client)
    add_subdirectory(applauncher)
endif()

if(withMobileClient)
    add_subdirectory(mobile_client)
endif()
