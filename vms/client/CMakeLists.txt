if(withDesktopClient OR withMobileClient)
    add_subdirectory(nx_vms_client_core)
endif()

if(withDesktopClient)
    find_package(Qt5
        COMPONENTS
            OpenGL
            Svg
            QuickWidgets
            WebKit
            WebKitWidgets)

    if(LINUX)
        find_package(Qt5 COMPONENTS X11Extras)
    endif()

    if(WINDOWS)
        if(withMiniLauncher)
            add_subdirectory(minilauncher)
        endif()
        add_subdirectory(nov_file_launcher)
    endif()

    add_subdirectory(nx_vms_client_desktop)
    add_subdirectory(desktop_client)
    add_subdirectory(applauncher)

    if (withTests)
        project(desktop_client_with_tests)
        add_custom_target(desktop_client_with_tests
            DEPENDS
                desktop_client
                common_ut
                client_core_ut
                client_ut)
    endif()

endif()

if(withMobileClient)
    add_subdirectory(mobile_client)
endif()

if(withScreenChecker)
    add_subdirectory(screen_checker)
endif()

if(withNovBrowser)
    add_subdirectory(nov_browser)
endif()
