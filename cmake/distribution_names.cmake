function(set_distribution_names)
    set(prefix ${installer.name})

    # Allowed values are: win64, win86, linux64, linux86, linux-arm64, mac, android, ios,
    # bpi, rpi, bananapi, isd, isd_s2.
    # linux64 and linux86 are planned to be renamed to linux-x64 and linux-x86 accordingly.
    set(distribution_platform "${targetDevice}")
    if(platform STREQUAL "windows")
        if(arch STREQUAL "x64")
            set(distribution_platform "win64")
        else()
            set(distribution_platform "win86")
        endif()
    elseif(platform STREQUAL "linux")
        if(arch STREQUAL "x64")
            set(distribution_platform "linux64")
        elseif(arch STREQUAL "x86")
            set(distribution_platform "linux86")
        elseif(arch STREQUAL "arm64")
            set(distribution_platform "linux-arm64")
        else()
            set(distribution_platform "${box}")
        endif()
    elseif(platform STREQUAL "macosx")
        set(distribution_platform "mac")
    elseif(platform STREQUAL "android")
        set(distribution_platform "android")
    elseif(platform STREQUAL "ios")
        set(distribution_platform "ios")
    endif()

    if(beta)
        set(beta_suffix "-beta")
        set(suffix "${targetDevice}${beta_suffix}-${cloudGroup}")
        if(targetDevice STREQUAL "linux_arm32")
            set(suffix_rpi "rpi${beta_suffix}-${cloudGroup}")
            set(suffix_bananapi "bananapi${beta_suffix}-${cloudGroup}")
        endif()
    else()
        set(beta_suffix)
        set(suffix "${targetDevice}")
        if(targetDevice STREQUAL "linux_arm32")
            set(suffix_rpi "rpi")
            set(suffix_bananapi "bananapi")
        endif()
    endif()

    set(client_distribution_name
        "${prefix}-client-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(server_distribution_name
        "${prefix}-server-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(bundle_distribution_name
        "${prefix}-bundle-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(servertool_distribution_name
        "${prefix}-servertool-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(client_update_distribution_name
        "${prefix}-client_update-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(server_update_distribution_name
        "${prefix}-server_update-${releaseVersion.full}-${suffix}" PARENT_SCOPE)

    if(targetDevice STREQUAL "linux_arm32")
        set(server_update_distribution_name_rpi
            "${prefix}-server_update-${releaseVersion.full}-${suffix_rpi}" PARENT_SCOPE)
        set(server_update_distribution_name_bananapi
            "${prefix}-server_update-${releaseVersion.full}-${suffix_bananapi}" PARENT_SCOPE)
    endif()

    set(cdb_distribution_name
        "${prefix}-cdb-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(hpm_distribution_name
        "${prefix}-hpm-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(client_debug_distribution_name
        "${prefix}-client_debug-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(server_debug_distribution_name
        "${prefix}-server_debug-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(misc_debug_distribution_name
        "${prefix}-misc_debug-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(libs_debug_distribution_name
        "${prefix}-libs_debug-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(cloud_debug_distribution_name
        "${prefix}-cloud_debug-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(qt_debug_distribution_name
        "${prefix}-qt_debug-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(testcamera_distribution_name
        "${prefix}-testcamera-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(unit_tests_distribution_name
        "${prefix}-unit_tests-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(mobile_client_distribution_name
        "${prefix}-client-${mobileClientVersion.full}-${suffix}" PARENT_SCOPE)
    set(analytics_sdk_distribution_name
        "${prefix}-analytics_sdk-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(ssc_analytics_plugin_distribution_name
        "${prefix}-ssc_analytics_plugin-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(product_distribution_name
        "${prefix}" PARENT_SCOPE)

    if(net2Version)
        set(paxton_plugin_distribution_name
            "${prefix}-paxton_plugin-${releaseVersion.full}-${net2Version}" PARENT_SCOPE)
    endif()

endfunction()

set_distribution_names()
