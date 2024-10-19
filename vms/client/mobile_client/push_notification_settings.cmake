## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function(setup_push_notification_settings)
    cmake_parse_arguments(CFG "" "FIREBASE_TEST_SETTINGS_FILE;BAIDU_TEST_API_KEY_FILE" "" ${ARGN})

    set(defaultPushNotificationProvider "firebase")
    nx_option(useProdNotificationSettings "Use prod settings for push notifications" OFF)
    set(pushNotificationProvider ${defaultPushNotificationProvider} CACHE STRING
        "Provider for the push notifications. Can be 'firebase' or 'baidu'.")

    nx_expose_variables_to_parent_scope(
        defaultPushNotificationProvider
        pushNotificationProvider
        useProdNotificationSettings
    )

    if(pushNotificationProvider STREQUAL "firebase")
        if(ANDROID)
            if(useProdNotificationSettings)
                set(firebasePushNotificationSettingsFile
                    "${customization_dir}/mobile_client/google-services.json")
            else()
                set(firebasePushNotificationSettingsFile "${CFG_FIREBASE_TEST_SETTINGS_FILE}" CACHE
                    FILEPATH "JSON file containing settings for firebase push notifications.")
            endif()

            if(NOT firebasePushNotificationSettingsFile
                    OR NOT EXISTS ${firebasePushNotificationSettingsFile})
                message(FATAL_ERROR
                    "firebasePushNotificationSettingsFile is empty or does not exists:"
                    "\"${firebasePushNotificationSettingsFile}\"")
            endif()

            nx_expose_to_parent_scope(firebasePushNotificationSettingsFile)
        endif()
    elseif(pushNotificationProvider STREQUAL "baidu")
        if(ANDROID)
            if(useProdNotificationSettings)
                set(baiduPushNotificationApiKey "${customization.mobile.android.baidu.apiKey}")
                if("${baiduPushNotificationApiKey}" STREQUAL "")
                    message(FATAL_ERROR "Baidu API key is not specified in the customization.")
                endif()
            else()
                set(baiduPushNotificationApiKeyFile "${CFG_BAIDU_TEST_API_KEY_FILE}" CACHE FILEPATH
                    "API key file for Baidu push notifications.")
                if(NOT baiduPushNotificationApiKeyFile
                        OR NOT EXISTS ${baiduPushNotificationApiKeyFile})
                    message(FATAL_ERROR
                        "baiduPushNotificationApiKeyFile is empty or does not exists:"
                        "\"${baiduPushNotificationApiKeyFile}\"")
                endif()

                file(READ ${baiduPushNotificationApiKeyFile} baiduPushNotificationApiKey)
                if("${baiduPushNotificationApiKey}" STREQUAL "")
                    message(FATAL_ERROR "File \"${baiduPushNotificationApiKeyFile}\" is empty.")
                endif()
                nx_expose_to_parent_scope(baiduPushNotificationApiKey)
            endif()
        endif()
    else()
        message(FATAL_ERROR "Invalid push notifications provider '${pushNotificationProvider}'.")
    endif()
endfunction()
