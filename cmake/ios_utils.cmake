## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(codeSigningIdentity "" CACHE STRING "Custom code signing identity for a developer build.")
include(CMakePrintHelpers)

set(fileSourceDir ${CMAKE_CURRENT_LIST_DIR})

function(_install_provisioning_profile profile_path profile_id system_provisioning_profiles_dir)
    if(NOT EXISTS ${system_provisioning_profiles_dir})
        return()
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy
            "${profile_path}"
            "${system_provisioning_profiles_dir}/${profile_id}.mobileprovision"
        RESULT_VARIABLE result
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot install provisioning profile \"${profile_path}\"")
    endif()
endfunction()

function(_prepare_provisioning_profile source_file_path ret_profile_id)
    execute_process(
        COMMAND
            "${fileSourceDir}/../build_utils/macos/get_provisioning_id.sh" "${source_file_path}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE profile_id
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot get provisioning profile id from \"${source_file_path}\"")
    endif()

    message(STATUS "Using provisioning profile: ${source_file_path}, id: ${profile_id}")
    set(${ret_profile_id} ${profile_id} PARENT_SCOPE)

    # Install to Xcode >= 16 location.
    _install_provisioning_profile(${source_file_path} ${profile_id}
        "$ENV{HOME}/Library/Developer/Xcode/UserData/Provisioning Profiles")
    # Install to Xcode < 16 location.
    _install_provisioning_profile(${source_file_path} ${profile_id}
        "$ENV{HOME}/Library/MobileDevice/Provisioning Profiles")
endfunction()

function(nx_prepare_ios_signing target profile_name app_id)
    set(app_dir "$<TARGET_FILE_DIR:${target}>")

    if(codeSigning)
        set(codeSigningIdentity ${customization.mobile.ios.signIdentity})
        set(provisioning_profiles_dir
            "${appleCodeSigningAssetsDir}/${customization}/ios/provisioning_profiles")

        _prepare_provisioning_profile(
            ${provisioning_profiles_dir}/${profile_name}.mobileprovision
            provisioning_profile_id)

        add_custom_command(TARGET ${target} PRE_LINK
            COMMAND ${CMAKE_COMMAND} -E copy
            "${provisioning_profiles_dir}/${profile_name}.mobileprovision"
            "${app_dir}/embedded.mobileprovision"
        )

        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${codeSigningIdentity}"
            XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--keychain ${codeSigningKeychainName}"
            XCODE_ATTRIBUTE_PROVISIONING_PROFILE "${provisioning_profile_id}"
        )
    elseif(developerBuild)
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
            # The identity should be set to this value to use automatic signing.
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${appleTeamId}"
        )
    else()
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        )
    endif()

    if(codeSigning OR developerBuild)
        message(STATUS
            "Prepeared code signing for target \"${target}\"."
            "\nCode signing identity: ${codeSigningIdentity}")
    endif()
endfunction()
