macro(_set_version pkg version)
    set(_${pkg}_version ${version})
endmacro()

function(nx_detect_package_versions)
    _set_version(qt "5.6.2")
    _set_version(boost "1.60.0")
    _set_version(openssl "1.0.2e")
    _set_version(ffmpeg "3.1.1")
    _set_version(quazip "0.7.1")
    _set_version(onvif "2.1.2-io2")
    _set_version(sigar "1.7")
    _set_version(openldap "2.4.42")
    _set_version(sasl2 "2.1.26")
    _set_version(openal "1.16")
    _set_version(libjpeg-turbo "1.4.2")
    _set_version(festival "2.4")
    _set_version(gtest "1.7.0")
    _set_version(gmock "1.7.0")
    _set_version(directx "JUN2010")

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
        _set_version(quazip "0.7.3")
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
    endif()

    if(box MATCHES "bpi|bananapi")
        _set_version(openssl "1.0.0j")
        _set_version(quazip "0.7")
    endif()

    if(box STREQUAL "bananapi")
        _set_version(ffmpeg "3.1.1-bananapi")
        _set_version(qt "5.6.1")
        _set_version(openssl "1.0.0j")
    endif()

    if(box STREQUAL "rpi")
        _set_version(qt "5.6.1")
        _set_version(quazip "0.7.2")
        _set_version(openssl "1.0.0j")
    endif()

    if(box STREQUAL "edge1")
        _set_version(qt "5.6.1")
        _set_version(openssl "1.0.1f")
        _set_version(quazip "0.7.2")
    endif()

    _set_version(festival-vox ${_festival_version})
    _set_version(help ${customization}-${releaseVersion.short})

    foreach(pkg
        qt
        boost
        openssl
        ffmpeg
        quazip
        onvif
        sigar
        openldap
        sasl2
        openal
        libjpeg-turbo
        festival festival-vox
        gtest gmock
        directx
        help
    )
        nx_set_variable_if_empty(${pkg}_version ${_${pkg}_version})
        nx_expose_to_parent_scope(${pkg}_version)
    endforeach()
endfunction()

function(nx_get_dependencies)
    if (WINDOWS OR (LINUX AND NOT ANDROID))
        set(haveServer TRUE)
    endif()

    if (WINDOWS OR MACOSX OR (LINUX AND NOT ANDROID AND box MATCHES "none|tx1"))
        set(haveDesktopClient TRUE)
    endif()

    if (box MATCHES "none|bpi")
        set(haveMobileClient TRUE)
    endif()

    if(WINDOWS OR MACOSX
        OR (LINUX AND NOT arch STREQUAL "x86" AND NOT ANDROID AND box MATCHES "none|bpi|tx1"))

        set(haveTests TRUE)
    endif()

    nx_rdep_add_package(qt PATH_VARIABLE QT_DIR)
    file(TO_CMAKE_PATH "${QT_DIR}" QT_DIR)
    set(QT_DIR ${QT_DIR} PARENT_SCOPE)

    nx_rdep_add_package(any/boost)

    if(haveServer OR haveDesktopClient OR haveTests)
        nx_rdep_add_package(any/qtservice)
        nx_rdep_add_package(any/qtsinglecoreapplication)
    endif()

    if(WIN32)
        set(nxKitLibraryType "SHARED" CACHE STRING "" FORCE)
    endif()
    nx_rdep_add_package(any/nx_kit)

    nx_rdep_add_package(openssl)
    nx_rdep_add_package(ffmpeg)

    if(box MATCHES "bpi|bananapi")
        nx_rdep_add_package(sysroot)
        nx_rdep_add_package(opengl-es-mali)
    endif()

    if(box MATCHES "rpi")
        nx_rdep_add_package(cifs-utils)
    endif()

    if(haveTests)
        nx_rdep_add_package(gtest)
        nx_rdep_add_package(gmock)
    endif()

    if(ANDROID OR WINDOWS OR box MATCHES "bpi")
        nx_rdep_add_package(openal)
    endif()

    if(NOT ANDROID AND NOT IOS)
        nx_rdep_add_package(quazip)
    endif()

    if(WINDOWS)
        nx_rdep_add_package(directx)
        nx_rdep_add_package("vcredist-2015" PATH_VARIABLE VC14RedistPath)
        set(VC14RedistPath ${VC14RedistPath} PARENT_SCOPE)
        nx_rdep_add_package("vmaxproxy-2.1")
        nx_rdep_add_package(windows/wix-3.11 PATH_VARIABLE wix_directory)
        set(wix_directory ${wix_directory} PARENT_SCOPE)
    endif()

    if(box STREQUAL "edge1")
        nx_rdep_add_package(cpro-1.0.0)
        nx_rdep_add_package(gdb)
    endif()

    if(haveDesktopClient)
        nx_rdep_add_package(any/qtsingleapplication)
        nx_rdep_add_package(any/help)
    endif()

    if(haveMobileClient)
        nx_rdep_add_package(any/qtsingleguiapplication)
    endif()

    if(haveDesktopClient OR haveMobileClient)
        nx_rdep_add_package(any/roboto-fonts)
    endif()

    if((haveServer OR haveDesktopClient) AND NOT box STREQUAL "edge1")
        nx_rdep_add_package(festival)
        if(NOT "${festival-vox_version}" STREQUAL "system")
            nx_rdep_add_package(any/festival-vox)
        endif()
    endif()

    if(ANDROID OR IOS)
        nx_rdep_add_package(libjpeg-turbo)
    endif()

    if(haveServer)
        nx_rdep_add_package(any/nx_sdk-1.6.0)
        nx_rdep_add_package(any/nx_storage_sdk-1.6.0)
        nx_rdep_add_package(onvif)
        nx_rdep_add_package(sigar)

        nx_rdep_add_package(any/apidoctool PATH_VARIABLE APIDOCTOOL_PATH)
        set(APIDOCTOOL_PATH ${APIDOCTOOL_PATH} PARENT_SCOPE)

        if(server-external_version)
            nx_rdep_add_package(any/server-external)
        else()
            nx_rdep_add_package(any/server-external-${branch} OPTIONAL
                PATH_VARIABLE server_external_path)
            if(NOT server_external_path)
                nx_rdep_add_package(any/server-external-${releaseVersion.short})
            endif()
        endif()

        if(LINUX AND arch MATCHES "arm|aarch64")
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

        nx_rdep_add_package(libstdc++-6.0.19)

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
