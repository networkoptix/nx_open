## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_subdirectory(nx_build_info)
add_subdirectory(nx_branding)
add_subdirectory(nx_fusion)

if(LINUX)
    add_subdirectory(libgl_stub)
endif()

add_subdirectory(nx_network)
add_subdirectory(nx_preprocessor)
add_subdirectory(nx_reflect)
add_subdirectory(nx_sql)
add_subdirectory(nx_utils)
add_subdirectory(nx_zip)

if(withDesktopClient)
    add_subdirectory(nx_pathkit)
endif()

if(withMediaServer OR withDesktopClient OR openSourceBuild)
    add_subdirectory(nx_monitoring)
endif()
