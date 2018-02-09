set(customWebAdminPackageDirectory "" CACHE STRING
    "Custom location of server-external package")

macro(_set_version pkg version)
    set(_${pkg}_version ${version})
endmacro()

function(nx_detect_package_versions)
    _set_version(qt "5.6.2")
    _set_version(boost "1.60.0")
    _set_version(openssl "1.0.2e")
    _set_version(ffmpeg "3.1.1")
    _set_version(sigar "1.7")
    _set_version(openldap "2.4.42")
    _set_version(sasl2 "2.1.26")
    _set_version(openal "1.16")
    _set_version(libjpeg-turbo "1.4.2")
    _set_version(festival "2.4")
    _set_version(directx "JUN2010")
    _set_version(cassandra "2.7.0")

    if(WINDOWS)
        _set_version(qt "5.6.1-1")
    endif()

    if(LINUX AND box STREQUAL "none")
        _set_version(qt "5.6.2-2")
    endif()

    if(MACOSX)
        _set_version(qt "5.6.3")
        _set_version(ffmpeg "3.1.1-2")
        _set_version(openssl "1.0.2e-2")
        _set_version(festival "2.1")
    endif()

    if(ANDROID)
        _set_version(qt "5.6.2-2")
        _set_version(openssl "1.0.2g")
        _set_version(openal "1.17.2")
    endif()

    if(IOS)
        _set_version(openssl "1.0.1i")
        _set_version(libjpeg-turbo "1.4.1")
        _set_version(ffmpeg "3.4")
    endif()

    if(box MATCHES "bpi|bananapi")
        _set_version(openssl "1.0.0j")
    endif()

    if(box STREQUAL "bananapi")
        _set_version(ffmpeg "3.1.1-bananapi")
        _set_version(qt "5.6.1-1")
        _set_version(openssl "1.0.0j")
    endif()

    if(box STREQUAL "rpi")
        _set_version(qt "5.6.1")
        _set_version(openssl "1.0.1t-deb8")
    endif()

    if(box STREQUAL "edge1")
        _set_version(qt "5.6.3")
        _set_version(openssl "1.0.1f")
    endif()

    if(box STREQUAL "tx1")
        _set_version(festival "2.1x")
        _set_version(openssl "1.0.0j")
        _set_version(qt "5.6.3")
    endif()

    foreach(pkg
        qt
        boost
        openssl
        ffmpeg
        sigar
        openldap
        sasl2
        openal
        libjpeg-turbo
        festival
        directx
        cassandra
    )
        if("${_${pkg}_version}" STREQUAL "")
            message(WARNING
                "Cannot set version of ${pkg} because _${pkg}_version variable is not set.")
        else()
            nx_set_variable_if_empty(${pkg}_version ${_${pkg}_version})
            nx_expose_to_parent_scope(${pkg}_version)
        endif()
    endforeach()
endfunction()

function(nx_get_dependencies)
    if (WINDOWS OR (LINUX AND NOT ANDROID))
        set(haveServer TRUE)
    endif()

    if ((LINUX AND arch STREQUAL "x64") OR (WINDOWS AND arch STREQUAL "x64") OR MACOSX)
        nx_rdep_add_package(cassandra)
    endif()

    if (WINDOWS OR MACOSX OR (LINUX AND box MATCHES "none|tx1"))
        set(haveDesktopClient TRUE)
    endif()

    if (box MATCHES "none|bpi")
        set(haveMobileClient TRUE)
    endif()

    if(WINDOWS OR MACOSX
        OR (LINUX AND NOT arch STREQUAL "x86" AND box MATCHES "none|bpi|tx1"))

        set(haveTests TRUE)
    endif()

    nx_rdep_add_package(qt PATH_VARIABLE QT_DIR)
    file(TO_CMAKE_PATH "${QT_DIR}" QT_DIR)
    set(QT_DIR ${QT_DIR} PARENT_SCOPE)

    nx_rdep_add_package(any/boost)

    nx_rdep_add_package(any/detection_plugin_interface)

    nx_rdep_add_package(openssl)
    nx_rdep_add_package(ffmpeg)

    if(box MATCHES "bpi|bananapi")
        nx_rdep_add_package(sysroot)
        nx_rdep_add_package(opengl-es-mali)
    endif()

    if(box MATCHES "rpi")
        nx_rdep_add_package(cifs-utils)
    endif()

    if(box STREQUAL "tx1")
        nx_rdep_add_package(sysroot)
        nx_rdep_add_package(tegra_video)
        nx_rdep_add_package(jetpack)
    endif()

    if(ANDROID OR WINDOWS OR box MATCHES "bpi")
        nx_rdep_add_package(openal)
    endif()


    if(LINUX AND box STREQUAL "none")
        nx_rdep_add_package(cifs-utils)
        nx_rdep_add_package(appserver-2.2.1)
    endif()

    if(WINDOWS)
        nx_rdep_add_package(directx)
        nx_rdep_add_package(vcredist-2015 PATH_VARIABLE vcredist_directory)
        set(vcredist_directory ${vcredist_directory} PARENT_SCOPE)
        nx_rdep_add_package(vmaxproxy-2.1)
        nx_rdep_add_package(windows/wix-3.11 PATH_VARIABLE wix_directory)
        set(wix_directory ${wix_directory} PARENT_SCOPE)
        nx_rdep_add_package(windows/signtool PATH_VARIABLE signtool_directory)
        set(signtool_directory ${signtool_directory} PARENT_SCOPE)
    endif()

    if(box STREQUAL "edge1")
        nx_rdep_add_package(cpro-1.0.0-1)
        nx_rdep_add_package(gdb)
    endif()

    if(haveDesktopClient)
        nx_rdep_add_package(any/help-${customization}-3.1 PATH_VARIABLE help_directory)
        set(help_directory ${help_directory} PARENT_SCOPE)
    endif()

    if(haveDesktopClient OR haveMobileClient)
        nx_rdep_add_package(any/roboto-fonts PATH_VARIABLE fonts_directory)
        set(fonts_directory ${fonts_directory} PARENT_SCOPE)
    endif()

    if((haveServer OR haveDesktopClient) AND NOT box STREQUAL "edge1")
        nx_rdep_add_package(festival)
        nx_rdep_add_package(any/festival-vox-${festival_version}
            PATH_VARIABLE festival_vox_directory)
        nx_expose_to_parent_scope(festival_vox_directory)
    endif()

    if(ANDROID OR IOS)
        nx_rdep_add_package(libjpeg-turbo)
    endif()

    if(haveServer)
        nx_rdep_add_package(any/nx_sdk-1.6.0)
        nx_rdep_add_package(any/nx_storage_sdk-1.6.0)
        nx_rdep_add_package(sigar)

        nx_rdep_add_package(any/apidoctool PATH_VARIABLE APIDOCTOOL_PATH)
        set(APIDOCTOOL_PATH ${APIDOCTOOL_PATH} PARENT_SCOPE)

        if(customWebAdminPackageDirectory)
            nx_copy_package(${customWebAdminPackageDirectory})
        elseif(server-external_version)
            nx_rdep_add_package(any/server-external)
        else()
            nx_rdep_add_package(any/server-external-${branch} OPTIONAL
                PATH_VARIABLE server_external_path)
            if(NOT server_external_path)
                nx_rdep_add_package(any/server-external-vms)
            endif()
        endif()

        if(LINUX AND arch MATCHES "arm")
            nx_rdep_add_package(openldap)
            nx_rdep_add_package(sasl2)
        endif()
    endif()

    if(box STREQUAL "bpi")
        nx_rdep_add_package(libvdpau-sunxi-1.0-deb7)
        nx_rdep_add_package(proxy-decoder-deb7)
        nx_rdep_add_package(ldpreloadhook-1.0-deb7)
        nx_rdep_add_package(libpixman-0.34.0-deb7)
        nx_rdep_add_package(libcedrus-1.0-deb7)

        nx_rdep_add_package(fontconfig-2.11.0)
        nx_rdep_add_package(additional-fonts)
        nx_rdep_add_package(libvdpau-1.0.4.1)

        nx_rdep_add_package(read-edid-3.0.2)
        nx_rdep_add_package(a10-display)
        nx_rdep_add_package(uboot-2014.04-10733-gbb5691c-dirty-vanilla)
    endif()

    nx_rdep_add_package("any/certificates-${customization}" PATH_VARIABLE certificates_path)
    set(certificates_path ${certificates_path} PARENT_SCOPE)
endfunction()

nx_detect_package_versions()
nx_get_dependencies()
