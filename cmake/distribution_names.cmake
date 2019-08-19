function(set_distribution_names)
    set(prefix ${installer.name})

    # Allowed values are: win64, win86, linux64, linux86, mac, android, ios,
    #  bpi, rpi, bananapi, tx1, isd, isd_s2
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
        else()
            set(distribution_platform "${box}")
        endif()
    elseif(platform STREQUAL "macosx")
        set(distribution_platform "mac")
    elseif(platform STREQUAL "android")
        if(arch STREQUAL "arm")
            set(distribution_platform "android_arm32")
        else()
            set(distribution_platform "android_${arch}")
        endif()
    elseif(platform STREQUAL "ios")
        set(distribution_platform "ios")
    endif()

    if(beta)
        set(beta_suffix "-beta")
        set(suffix "${distribution_platform}${beta_suffix}-${cloudGroup}")
    else()
        set(beta_suffix)
        set(suffix "${distribution_platform}")
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
