## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(withDesktopClient OR withMobileClient)
    find_package(Qt6 COMPONENTS Svg)

    add_subdirectory(nx_vms_client_core)
endif()

if(withDesktopClient)
    if(WINDOWS)
        add_subdirectory(nov_file_launcher)
        if(withMiniLauncher)
            add_subdirectory(minilauncher)
        endif()
    endif()

    add_subdirectory(applauncher)
    add_subdirectory(desktop_client)
    add_subdirectory(libvms_gateway_core)
    add_subdirectory(nx_vms_client_desktop)
endif()
