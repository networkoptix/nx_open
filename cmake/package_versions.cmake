function(detect_package_versions)
    if(WIN32)
        set(_qt_version "5.6.1")
    else()
        set(_qt_version "5.6.2")
    endif()
    set(_boost_version "1.60.0")
    set(_openssl_version "1.0.2e")
    set(_ffmpeg_version "3.1.1")
    set(_quazip_version "0.7.1")
    set(_onvif_version "2.1.2-io2")
    set(_sigar_version "1.7")
    set(_openldap_version "2.4.42")
    set(_sasl_version "2.1.26")
    set(_openal_version "1.16")
    set(_libjpeg-turbo_version "1.4.2")
    set(_libcreateprocess_version "0.1")
    set(_festival_version "2.4")
    set(_vmux_version "1.0.0")
    set(_gtest_version "1.7.0")
    set(_gmock_version "1.7.0")
    set(_gcc_version "4.9.4")

    if(MACOSX)
        set(_quazip_version "0.7.2")
        set(_festival_version "2.1")
    endif()

    if(ANDROID)
        set(_openssl_version "1.0.2g")
        set(_openal_version "1.17.2")
    endif()

    set(qt_version ${_qt_version} CACHE STRING "")
    set(boost_version ${_boost_version} CACHE STRING "")
    set(openssl_version ${_openssl_version} CACHE STRING "")
    set(ffmpeg_version ${_ffmpeg_version} CACHE STRING "")
    set(quazip_version ${_quazip_version} CACHE STRING "")
    set(onvif_version ${_onvif_version} CACHE STRING "")
    set(sigar_version ${_sigar_version} CACHE STRING "")
    set(openldap_version ${_openldap_version} CACHE STRING "")
    set(sasl_version ${_sasl_version} CACHE STRING "")
    set(openal_version ${_openal_version} CACHE STRING "")
    set(libjpeg-turbo_version ${_libjpeg-turbo_version} CACHE STRING "")
    set(libcreateprocess_version ${_libcreateprocess_version} CACHE STRING "")
    set(festival_version ${_festival_version} CACHE STRING "")
    set(festival-vox_version ${festival_version} PARENT_SCOPE)
    set(vmux_version ${_vmux_version} CACHE STRING "")
    set(gtest_version ${_gtest_version} CACHE STRING "")
    set(gmock_version ${_gmock_version} CACHE STRING "")
    set(gcc_version ${_gcc_version} CACHE STRING "")

    set(help_version "${customization}-${releaseVersion}" PARENT_SCOPE)
endfunction()

detect_package_versions()
