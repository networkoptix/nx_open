if(targetDevice)
    if(targetDevice MATCHES "bpi|bananapi")
        nx_rdep_sync_package(bpi/gcc-linaro-7.2.1)
        set(toolchain_directory "${PACKAGES_DIR}/bpi/gcc-linaro-7.2.1")
    elseif(targetDevice MATCHES "rpi|edge1")
        nx_rdep_sync_package(linux-arm/gcc-linaro-7.2.1)
        nx_rdep_sync_package(linux-arm/glib-2.0)
        nx_rdep_sync_package(linux-arm/zlib-1.2)
        set(toolchain_directory "${PACKAGES_DIR}/linux-arm/gcc-linaro-7.2.1")
    elseif(targetDevice MATCHES "android")
        if("$ENV{ANDROID_NDK}" STREQUAL "" OR NOT IS_DIRECTORY "$ENV{ANDROID_NDK}")
            nx_rdep_sync_package(android/android-ndk)
        endif()
        if("$ENV{ANDROID_HOME}" STREQUAL "" OR NOT IS_DIRECTORY "$ENV{ANDROID_HOME}")
            nx_rdep_sync_package(android/android-sdk)
        endif()
    endif()
endif()
