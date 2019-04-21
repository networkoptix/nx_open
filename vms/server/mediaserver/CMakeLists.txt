if(LINUX)
    # In Linux we apply capabilities to server binary (setcap CAP_NET_BIND_SERVICE=+eip). This
    # disables "$ORIGIN" processing in RPATH, this server cannot find libraries. The simplest
    # solution is to add an absolute libs path because we know it. The "$ORIGIN/../lib" stays here
    # too to simplify server execution by developers.
    list(APPEND CMAKE_INSTALL_RPATH "/opt/${deb.customization.company.name}/mediaserver/lib")
    list(APPEND CMAKE_INSTALL_RPATH "/opt/${deb.customization.company.name}/mediaserver/lib/ffmpeg")
endif()

set(file_internal_name "${mediaserver_application_name}")
set(file_description "${mediaserver.display.name}")

nx_add_target(mediaserver EXECUTABLE SIGNED
    PUBLIC_LIBS nx_vms_server)

if(TARGET festival_libs)
    target_link_libraries(mediaserver PRIVATE festival_libs)
endif()

if(LINUX)
    # Security requirement (see hardening-check).
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pie")
endif()
