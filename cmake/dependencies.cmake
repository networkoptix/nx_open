function(detect_package_versions)
    set(_qt_version "5.6.2")
    set(_boost_version "1.60.0")
    set(_openssl_version "1.0.2e")
    set(_ffmpeg_version "3.1.1")
    set(_quazip_version "0.7.1")
    set(_onvif_version "2.1.2-io2")
    set(_sigar_version "1.7")
    set(_openldap_version "2.4.42")
    set(_sasl2_version "2.1.26")
    set(_openal_version "1.16")
    set(_libjpeg-turbo_version "1.4.2")
    set(_festival_version "2.4")
    set(_gtest_version "1.7.0")
    set(_gmock_version "1.7.0")
    set(_directx_version "JUN2010")

    if(WINDOWS)
        set(_qt_version "5.6.1-1")
    endif()

    if(LINUX AND box STREQUAL "none")
        set(_qt_version "5.6.2-2")
    endif()

    if(MACOSX)
        set(_qt_version "5.6.2-2")
        set(_ffmpeg_version "3.1.1-2")
        set(_openssl_version "1.0.2e-2")
        set(_quazip_version "0.7.3")
        set(_festival_version "2.1")
    endif()

    if(ANDROID)
        set(_qt_version "5.6.2-2")
        set(_openssl_version "1.0.2g")
        set(_openal_version "1.17.2")
    endif()

    if(IOS)
        set(_openssl_version "1.0.1i")
        set(_libjpeg-turbo_version "1.4.1")
    endif()

    if(box MATCHES "bpi|bananapi")
        set(_openssl_version "1.0.0j")
        set(_quazip_version "0.7")
    endif()

    if(box STREQUAL "bananapi")
        set(_ffmpeg_version "3.1.1-bananapi")
        set(_qt_version "5.6.1")
        set(_openssl_version "1.0.0j")
    endif()

    if(box STREQUAL "rpi")
        set(_qt_version "5.6.1")
        set(_quazip_version "0.7.2")
        set(_openssl_version "1.0.0j")
        set(_festival_version "2.4")
    endif()

    if(box STREQUAL "edge1")
        set(_qt_version "5.6.1")
        set(_openssl_version "1.0.1f")
        set(_quazip_version "0.7.2")
    endif()

    set(qt_version ${_qt_version} CACHE STRING "")
    set(boost_version ${_boost_version} CACHE STRING "")
    set(openssl_version ${_openssl_version} CACHE STRING "")
    set(ffmpeg_version ${_ffmpeg_version} CACHE STRING "")
    set(quazip_version ${_quazip_version} CACHE STRING "")
    set(onvif_version ${_onvif_version} CACHE STRING "")
    set(sigar_version ${_sigar_version} CACHE STRING "")
    set(openldap_version ${_openldap_version} CACHE STRING "")
    set(sasl2_version ${_sasl2_version} CACHE STRING "")
    set(openal_version ${_openal_version} CACHE STRING "")
    set(libjpeg-turbo_version ${_libjpeg-turbo_version} CACHE STRING "")
    set(festival_version ${_festival_version} CACHE STRING "")
    set(festival-vox_version ${festival_version} PARENT_SCOPE)
    set(gtest_version ${_gtest_version} CACHE STRING "")
    set(gmock_version ${_gmock_version} CACHE STRING "")
    set(directx_version ${_directx_version} CACHE STRING "")
    set(server-external_version "" CACHE STRING "")

    set(help_version "${customization}-${releaseVersion.short}" PARENT_SCOPE)
endfunction()

function(get_dependencies)
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

    nx_rdep_add_package(any/nx_kit)

    nx_rdep_add_package(openssl)
    nx_rdep_add_package(ffmpeg)

    if(box MATCHES "bpi|bananapi")
        nx_rdep_add_package(sysroot)
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
    endif()

    if(box STREQUAL "edge1")
        nx_rdep_add_package(cpro-1.0.0)
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
        nx_rdep_add_package(opengl-es-mali)
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

detect_package_versions()
get_dependencies()
