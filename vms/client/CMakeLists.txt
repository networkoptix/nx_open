## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(withDesktopClient OR withMobileClient)
    find_package(Qt6 COMPONENTS Multimedia Quick Svg REQUIRED)

    add_subdirectory(nx_vms_client_core)
endif()

if(withDesktopClient)
    set(app_icons_include_path
        ${open_source_root}/vms/client/nx_vms_client_desktop/src/nx/vms/client/desktop/app_icons.h)

    if(WINDOWS)
        add_subdirectory(nov_file_launcher)
        if(withMiniLauncher)
            add_subdirectory(minilauncher)
        endif()
    endif()

    add_subdirectory(applauncher)
    add_subdirectory(desktop_client)
    add_subdirectory(nx_vms_client_desktop)
endif()

if(withDesktopClient OR withTests)
    add_subdirectory(libvms_gateway_core)
endif()

if(withMobileClient)
    add_subdirectory(../../../open_candidate/vms/client/mobile_client ${CMAKE_BINARY_DIR}/mobile_client)
endif()
