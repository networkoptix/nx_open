## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

function(set_distribution_names)
    set(prefix ${customization.installerName})

    # Release distributions must not have "-release" suffix.
    if(${publicationType} STREQUAL "release")
        set(publication_type_suffix "")
    else()
        set(publication_type_suffix "-${publicationType}")
    endif()
    nx_expose_to_parent_scope(publication_type_suffix)

    if("${flavor}" STREQUAL "default" OR "${flavor}" STREQUAL "")
        set(flavor_suffix "")
    else()
        set(flavor_suffix ".${flavor}")
    endif()

    # Only private builds must have cloud group suffix.
    if(${publicationType} STREQUAL "private")
        set(cloud_group_suffix "-${cloudGroup}")
    else()
        set(cloud_group_suffix)
    endif()

    if(ANDROID AND NOT "${pushNotificationProvider}" STREQUAL "${defaultPushNotificationProvider}")
        set(notification_provider_suffix "-${pushNotificationProvider}")
    endif()

    set(distribution_name_suffix "${publication_type_suffix}${cloud_group_suffix}")
    set(suffix
        "${targetDevice}${flavor_suffix}${distribution_name_suffix}${notification_provider_suffix}"
    )
    set(android_aab_suffix "android${distribution_name_suffix}${notification_provider_suffix}")
    set(vmsBenchmarkSuffix "${targetDevice}${publication_type_suffix}")
    set(sdkSuffix "universal${publication_type_suffix}")
    set(webadminSuffix "universal${publication_type_suffix}")

    if(NOT "${customization.desktop.customClientVariant}" STREQUAL "")
        set(custom_client_suffix "-${customization.desktop.customClientVariant}")
    else()
        set(custom_client_suffix "")
    endif()

    set(client_distribution_name
        "${prefix}-client${custom_client_suffix}-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(server_distribution_name
        "${prefix}-server-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(bundle_distribution_name
        "${prefix}-bundle-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(servertool_distribution_name
        "${prefix}-servertool-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(client_update_distribution_name
        "${prefix}-client_update${custom_client_suffix}-${releaseVersion.full}-${suffix}"
        PARENT_SCOPE)
    set(server_update_distribution_name
        "${prefix}-server_update-${releaseVersion.full}-${suffix}" PARENT_SCOPE)

    set(cdb_distribution_name
        "${prefix}-cdb-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(hpm_distribution_name
        "${prefix}-hpm-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(client_debug_distribution_name
        "${prefix}-client_debug-${releaseVersion.full}-${suffix}" PARENT_SCOPE)
    set(mobile_client_debug_distribution_name
        "${prefix}-client_debug-${mobileClientVersion.full}-${suffix}" PARENT_SCOPE)
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
    set(mobile_client_aab_distribution_name
        "${prefix}-client-${mobileClientVersion.full}-${android_aab_suffix}" PARENT_SCOPE)
    set(webadmin_distribution_name
        "${prefix}-webadmin-${releaseVersion.full}-${webadminSuffix}" PARENT_SCOPE)
    set(metadata_sdk_distribution_name
        "${prefix}-metadata_sdk-${releaseVersion.full}-${sdkSuffix}" PARENT_SCOPE)
    set(video_source_sdk_distribution_name
        "${prefix}-video_source_sdk-${releaseVersion.full}-${sdkSuffix}" PARENT_SCOPE)
    set(storage_sdk_distribution_name
        "${prefix}-storage_sdk-${releaseVersion.full}-${sdkSuffix}" PARENT_SCOPE)
    set(cloud_storage_sdk_distribution_name
        "${prefix}-cloud_storage_sdk-${releaseVersion.full}-${sdkSuffix}" PARENT_SCOPE)
    set(vms_benchmark_distribution_name
        "${prefix}-vms_benchmark-${releaseVersion.full}-${vmsBenchmarkSuffix}" PARENT_SCOPE)
    set(ssc_analytics_plugin_distribution_name
        "${prefix}-ssc_analytics_plugin-${releaseVersion.full}-${suffix}" PARENT_SCOPE)

    set(conan_refs_distribution_name
        "${prefix}-conan_refs-${suffix}" PARENT_SCOPE)

    if(net2Version)
        set(paxton_plugin_distribution_name
            "${prefix}-paxton_plugin-${releaseVersion.full}-${net2Version}" PARENT_SCOPE)
    endif()
endfunction()

set_distribution_names()
