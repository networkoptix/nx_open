## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt6 COMPONENTS Quick Svg WebChannel WebSockets WebView QuickControls2 REQUIRED)

set(project.version.code "${buildNumber}")
set(project.version.name "${mobileClientVersion}")
set(artifacts_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../artifacts)

include(push_notification_settings.cmake)

function(get_android_extra_libs result)
    list(GET OpenSSL_LIB_DIRS 0 openssl_lib_dir)
    set(openssl_libs
        ${openssl_lib_dir}/libcrypto.so
        ${openssl_lib_dir}/libssl.so
    )

    list(GET ffmpeg_LIB_DIRS 0 ffmpeg_lib_dir)
    set(ffmpeg_libs "")
    foreach(lib ${ffmpeg_LIBRARY_LIST})
        list(APPEND ffmpeg_libs "${ffmpeg_lib_dir}/lib${lib}.so")
    endforeach()

    list(GET openal_LIB_DIRS 0 openal_lib_dir)
    set(openal_libs ${openal_lib_dir}/libopenal.so)

    set(quazip_libs "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libquazip.so")

    set(${result} ${openssl_libs} ${ffmpeg_libs} ${openal_libs} ${quazip_libs} PARENT_SCOPE)
endfunction()

function(copy_folder source_dir target_dir)
    file(GLOB sources_list "${source_dir}/*")
    foreach(item ${sources_list})
        file(COPY "${item}" DESTINATION ${target_dir})
    endforeach()
endfunction()

function(prepare_android_package_sources source_dir target_dir)
    copy_folder(${source_dir} ${target_dir})

    # Removes excess files and keeps only needed for specified provider support.
    set(push_java_dir "${target_dir}/src/com/nxvms/mobile/push")
    set(excessiveProviders "firebase" "baidu")
    list(REMOVE_ITEM excessiveProviders ${pushNotificationProvider})
    foreach(excessiveProvider ${excessiveProviders})
        file(REMOVE_RECURSE "${push_java_dir}/${excessiveProvider}")
    endforeach()
endfunction()

function(prepare_push_manifest_variables source_path target_path)
    set(push_settings_variable_names
        push_meta_data
        push_extra_permissions
        push_service_settings)

    foreach(push_variable ${push_settings_variable_names})
        set(push_settings_file_name "${push_variable}.txt")
        set(source_push_settings_file_name
            "${source_path}/${pushNotificationProvider}/${push_settings_file_name}")

        if(EXISTS ${source_push_settings_file_name})
            set(target_push_settings_file_name "${target_path}/${push_settings_file_name}")
            nx_configure_file(${source_push_settings_file_name}
                ${target_push_settings_file_name})
            file(STRINGS ${target_push_settings_file_name} ${push_variable} NEWLINE_CONSUME)
            nx_expose_to_parent_scope(${push_variable})
        endif()
    endforeach()
endfunction()

file(GLOB_RECURSE QML_FILES CONFIGURE_DEPENDS static-resources/qml/*.qml static-resources/qml/*.js)

set(resources)

set(common_assets
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bytedance_iconpark.dat
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/client_core_external.dat
)

file(GLOB_RECURSE font_files ${CMAKE_BINARY_DIR}/bin/fonts/*.ttf)

set(dev_data_path "${CMAKE_CURRENT_LIST_DIR}/dev-data")

foreach(customUri ${customization.mobile.compatibleUriProtocols})
    if (ANDROID)
        set(extra_protocol_value "<data android:scheme=\"${customUri}\"/>")
    else()
        set(extra_protocol_value "<string>${customUri}</string>")
    endif()
    string(CONCAT compatibleProtocols
        ${compatibleProtocols}
        ${extra_protocol_value})
endforeach()

set(mobile_client_customization_dir "${customization_dir}/mobile_client")

setup_push_notification_settings()

if(IOS)
    if(NOT mobile_client_ios_launch_screen)
        set(mobile_client_ios_launch_screen
            ${CMAKE_CURRENT_LIST_DIR}/ios/Launch.xib)
    endif()

    set(ios_sources
        src/ui/window_utils_ios.mm
        src/utils/app_delegate.mm
        src/nx/vms/client/mobile/controllers/web_view_controller_ios.mm
        src/nx/vms/client/mobile/push_notification/details/ios/push_platform_helpers.mm
        src/nx/vms/client/mobile/push_notification/details/ios/push_permission_manager.mm
        src/nx/vms/client/mobile/push_notification/details/ios/token_data_provider.mm
    )

    set(icons_dir ${customization_dir}/icons/ios/ios/images)

    file(GLOB resources CONFIGURE_DEPENDS ${icons_dir}/logo*.png)
    list(APPEND resources ${mobile_client_ios_launch_screen})
    list(APPEND resources ${CMAKE_CURRENT_LIST_DIR}/ios/qt.conf)

    nx_copy(${CMAKE_CURRENT_SOURCE_DIR}/ios/mobile_client.xcassets
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(GLOB icons CONFIGURE_DEPENDS ${icons_dir}/icon_*.png)
    nx_copy(${icons}
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/mobile_client.xcassets/AppIcon.appiconset)
    list(APPEND resources ${CMAKE_CURRENT_BINARY_DIR}/mobile_client.xcassets)
else()
    set(ios_sources)
endif()

if(NOT ANDROID AND NOT IOS)
    # Dummy value for desktop builds.
    set(useProdNotificationSettings "true")
endif()

set(build_info_target_file ${CMAKE_CURRENT_BINARY_DIR}/build_info.cpp)
nx_configure_file(
    src/nx/vms/client/mobile/push_notification/details/build_info.cpp.in
    ${build_info_target_file})

set(customization_resources_dir ${mobile_client_customization_dir}/resources)
if(NOT EXISTS ${customization_resources_dir})
    set(customization_resources_dir)
endif()

set(target_args
    ROOT_FOLDER
    ADDITIONAL_SOURCES
        ${ios_sources}
        ${build_info_target_file}
    ADDITIONAL_RESOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/skins/${customization.skin}
        ${customization_resources_dir}
    OTHER_SOURCES
        ${QML_FILES}
        ${resources}
    PUBLIC_LIBS
        Qt6::WebChannel Qt6::WebSockets Qt6::Svg Qt6::WebView Qt6::QuickControls2
        nx_kit
        nx_vms_api
        nx_vms_common
        nx_vms_client_core)

if(ANDROID)
    set(CMAKE_SHARED_MODULE_SUFFIX_CXX "_${CMAKE_ANDROID_ARCH_ABI}.so")
    string(REPLACE "-Wl,--as-needed" "" CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS})
    nx_add_target(mobile_client LIBRARY LIBRARY_TYPE MODULE NO_API_MACROS ${target_args})
else()
    nx_add_target(mobile_client EXECUTABLE ${target_args})
endif()
nx_make_target_translatable(mobile_client COMPONENTS "mobile")

target_compile_definitions(mobile_client PRIVATE MOBILE_CLIENT_EXPORT=${API_EXPORT_MACRO})

nx_get_component_translation_list(translation_files "mobile" FULL_PATH)

if(ANDROID)
    find_package(openal REQUIRED)

    set(android_bin "${CMAKE_CURRENT_BINARY_DIR}/android")
    file(MAKE_DIRECTORY ${android_bin})

    # This file contains some variables used to build AAB.
    nx_configure_file(apk_build.conf.in ${CMAKE_CURRENT_BINARY_DIR}/apk_build.conf)

    set(android_push_dependencies)
    set(push_extra_libs)

    if(pushNotificationProvider STREQUAL "firebase")
        nx_exclude_sources_from_target(mobile_client
            src/nx/vms/client/mobile/push_notification/details/android/baidu)

        set(gradle_dep_lines "\
            implementation 'androidx.security:security-crypto:1.0.0'\n\
            implementation 'com.google.guava:guava:33.2.1-jre'\n\
            implementation(platform('com.google.firebase:firebase-bom:33.1.2'))\n\
            implementation('com.google.firebase:firebase-messaging')\n")

        nx_execute_process_or_fail(
            COMMAND ${PYTHON_EXECUTABLE}
                ${artifacts_dir}/firebase-cpp-sdk/generate_xml_from_google_services_json.py
                "-i${firebasePushNotificationSettingsFile}"
                "-o${android_bin}/res/values/firebase.xml")
        nx_store_known_file(${android_bin}/res/values/firebase.xml)
    elseif(pushNotificationProvider STREQUAL "baidu")
        if("${customization.mobile.android.baidu.displayName}" STREQUAL "")
            message("Custom display name for the application with Baidu provider is empty.")
        else()
            set(customization.mobile.displayName
                ${customization.mobile.android.baidu.displayName})
        endif()

        nx_exclude_sources_from_target(mobile_client
            src/nx/vms/client/mobile/push_notification/details/android/firebase)

        # Update package name and id to support multiple application instances on
        # a single device.
        set(customization.mobile.android.packageName
            "${customization.mobile.android.packageName}.${pushNotificationProvider}")

        set(baidu_push_sdk_dir ${CMAKE_CURRENT_BINARY_DIR}/baidu-push-sdk)
        file(DOWNLOAD "https://channelpush.cdn.bcebos.com/sdk/PushSDK-8.2.9.8.zip"
            ${baidu_push_sdk_dir}.zip
            EXPECTED_MD5 "49e684ece2e6832419ff04e9ebb18169")
        file(ARCHIVE_EXTRACT INPUT ${baidu_push_sdk_dir}.zip
            PATTERNS "*.aar"
            DESTINATION ${baidu_push_sdk_dir})
        file(GLOB_RECURSE android_push_dependencies "${baidu_push_sdk_dir}/*.aar")
        nx_store_known_files(${android_push_dependencies})

        set(gradle_dep_lines "\
            implementation 'androidx.security:security-crypto:1.0.0'\n\
            implementation 'com.google.guava:guava:32.1.2-jre'\n\
            implementation 'androidx.appcompat:appcompat:1.1.0'\n\
            implementation 'androidx.core:core:1.1.0'\n")
    endif()

    set(android_resources_source_dir
        "${CMAKE_CURRENT_SOURCE_DIR}/${bin_resources_path}/android")
    set(android_filter_resources_dir
        "${CMAKE_CURRENT_SOURCE_DIR}/android.in")

    # Configure step will put java files into template_dir.
    # The apk target also depends on those files, this way they are going to be recompiled
    # with the most recent changes.
    file(GLOB_RECURSE java_files ${android_filter_resources_dir}/*.java)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${java_files})

    copy_folder(${android_resources_source_dir} ${android_bin})
    prepare_android_package_sources(
        ${android_filter_resources_dir}
        ${android_bin})
    prepare_push_manifest_variables(
        ${android_filter_resources_dir}
        ${android_bin})

    nx_configure_file("${android_filter_resources_dir}/AndroidManifest.xml"
        "${android_bin}/AndroidManifest.xml")
    nx_configure_file("${android_filter_resources_dir}/res/values/strings.xml"
        "${android_bin}/res/values/strings.xml")
    nx_configure_file("${android_filter_resources_dir}/local.properties"
        "${android_bin}/local.properties")

    set(branding_file_path "/src/com/nxvms/mobile/utils/Branding.java")
    nx_configure_file("${android_filter_resources_dir}${branding_file_path}"
        "${android_bin}${branding_file_path}")

    set(template_dir ${CMAKE_CURRENT_BINARY_DIR}/mobile_client_apk_template)

    nx_add_merge_directories_target(mobile_client_apk_template
        DEPENDS ${common_assets} ${translation_files} ${font_files}
        OUTPUT ${template_dir}
        ${android_bin}
        ${CMAKE_CURRENT_SOURCE_DIR}/android
        ${customization_dir}/icons/android/android
        PREFIX assets/translations
            ${translation_files}
        PREFIX assets/fonts
            ${font_files}
        PREFIX assets
            ${common_assets}
    )
    add_dependencies(mobile_client mobile_client_apk_template)
    nx_store_known_files_in_directory(${template_dir})

    set(apk_file_name "${distribution_output_dir}/${mobile_client_distribution_name}.apk")

    if(codeSigning)
        set(code_signing_parameters
            KEYSTORE_FILE "${mobile_client_customization_dir}/key.store"
            KEYSTORE_ALIAS "${customization.mobile.android.alias}"
            KEYSTORE_PASSWORD "${customization.mobile.android.storepass}"
            KEYSTORE_KEY_PASSWORD "${customization.mobile.android.keypass}"
        )
    else()
        set(code_signing_parameters)
    endif()

    target_link_libraries(mobile_client PUBLIC
        GLESv2 z log)

    get_android_extra_libs(extra_libs)
    list(APPEND extra_libs ${push_extra_libs})
    add_android_apk(mobile_client_apk TARGET mobile_client
        DEPENDS ${java_files}
        FILE_NAME ${apk_file_name}
        TEMPLATE ${template_dir}
        QML_ROOT_PATH ${CMAKE_CURRENT_LIST_DIR}/static-resources/qml
        QML_IMPORT_PATHS ${open_source_root}/vms/client/nx_vms_client_core/static-resources/qml
        VERSION "${mobileClientVersion.full}"
        EXTRA_LIBS ${extra_libs}
        EXTRA_JAVA_LIBS ${android_push_dependencies}
        GRADLE_DEPENDENCIES ${gradle_dep_lines}
        ${code_signing_parameters}
    )

    prepare_qtcreator_android_target(mobile_client
        PACKAGE_SOURCE ${template_dir}
        QML_ROOT_PATH ${CMAKE_CURRENT_LIST_DIR}/static-resources/qml
        EXTRA_LIBS ${extra_libs}
    )
endif()

if(IOS)
    string(LENGTH ${customization.mobile.ios.bundleName} bundle_name_length)
    string(LENGTH ${customization.mobile.displayName} display_name_length)
    if(bundle_name_length GREATER display_name_length)
        message(FATAL_ERROR
            "customization.mobile.ios.bundleName must not be longer than customization.mobile.displayName.")
    endif()

    set (ios_filter_resources_dir ${CMAKE_CURRENT_SOURCE_DIR}/ios)
    if(appleTeamIdIsFree)
        set(entitlements_file "${ios_filter_resources_dir}/mobile_client-free.entitlements.in")
    elseif(developerBuild)
        set(entitlements_file "${ios_filter_resources_dir}/mobile_client-developer.entitlements.in")
    else()
        set(entitlements_file "${ios_filter_resources_dir}/mobile_client-release.entitlements.in")
    endif()

    nx_configure_file(${entitlements_file} ${CMAKE_CURRENT_BINARY_DIR}/mobile_client.entitlements)

    set_target_properties(mobile_client PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/ios/Info.plist.in"
        RESOURCE "${resources}" #";${CMAKE_CURRENT_BINARY_DIR}/mobile_client.xcassets"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${customization.mobile.ios.bundleIdentifier}"
        XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES FALSE
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
            "${CMAKE_CURRENT_BINARY_DIR}/mobile_client.entitlements"
        XCODE_GENERATE_SCHEME TRUE # Let Xcode know this is the app to run and hide other targets.
    )

    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        # Put dSYM files into a different directory to avoid signing issues.
        nx_process_macos_target_debug_symbols(mobile_client OUTPUT_DIR "${CMAKE_BINARY_DIR}")

        set(distribution_config_file "${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf")
        nx_configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/ios/build_distribution.conf.in
            ${distribution_config_file}
            @ONLY)

        set(client_debug_file
            "${distribution_output_dir}/${mobile_client_debug_distribution_name}.zip")

        nx_create_distribution_package(
            PACKAGE_NAME "client debug"
            PACKAGE_TARGET client_debug
            PACKAGE_GENERATION_SCRIPT_NAME ios/generate_client_debug_package.py
            OUTPUT_FILE ${client_debug_file}
            CONFIG_FILES ${distribution_config_file}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            DEPENDENCIES mobile_client push_notification_extension)
    endif()

    nx_prepare_ios_signing(mobile_client "production" ${customization.mobile.ios.bundleIdentifier})

    list(GET ffmpeg_LIB_DIRS 0 ffmpeg_lib_dir)

    setup_ios_application(mobile_client
        ASSETS ${common_assets}
        TRANSLATIONS ${translation_files}
    )

    add_subdirectory("ios/extension")

    target_link_libraries(mobile_client PRIVATE
        "-framework SafariServices"
        "-framework UserNotifications"
        "-framework VideoToolbox"
        utils
    )

    add_dependencies(mobile_client push_notification_extension)
    target_link_options(mobile_client PUBLIC "-ObjC")

    if(codeSigning)
        prepare_mac_keychain(mobile_client)
    endif()

    set(ipa_file_name "${distribution_output_dir}/${mobile_client_distribution_name}.ipa")

    add_ios_ipa(mobile_client_ipa TARGET mobile_client FILE_NAME ${ipa_file_name})

    set(app_dir "$<TARGET_FILE_DIR:mobile_client>")
    set(appex_dir "$<TARGET_FILE_DIR:push_notification_extension>")

    add_custom_command(TARGET mobile_client PRE_LINK
        COMMAND ${CMAKE_COMMAND}
            -E copy_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fonts ${app_dir}/fonts
        COMMENT "Copying fonts"
    )

    add_custom_command(TARGET mobile_client PRE_LINK
        COMMAND ${CMAKE_COMMAND}
            -E copy ${common_assets} ${app_dir}
        COMMENT "Copying common assets files"
    )

    add_custom_command(TARGET mobile_client
        POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E
        copy_directory "${appex_dir}/../push_notification_extension.appex"
        "${app_dir}/PlugIns/push_notification_extension.appex"
        MAIN_DEPENDENCY push_notification_extension)
endif()

# Add Qt Shaders.

file(GLOB_RECURSE shader_sources CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "static-resources/qml/*.vert"
   # "static-resources/qml/*.frag" - there is a single fragment shader that requires special handling.
)
# This command generates mobile_client_other_files target.
qt6_add_shaders(mobile_client "mobile_client_vertex_shaders"
    BATCHABLE
    PREFIX
        "/"
    BASE
        "static-resources"
    FILES
        ${shader_sources}
)
set_property(TARGET mobile_client_other_files PROPERTY FOLDER client/other_files)

set(projection_types "Stereographic;Equisolid;Equidistant")

foreach(lensProjectionType ${projection_types})
    foreach(viewProjectionType ${projection_types})
        set(unique_shader_suffix "${lensProjectionType}_${viewProjectionType}")
        qt6_add_shaders(mobile_client "mobile_client_fragment_shaders_${unique_shader_suffix}"
            BATCHABLE
            PREFIX
                "/"
            FILES
                "static-resources/qml/Nx/Screens/private/VideoScreen/FisheyeShaderEffect.frag"
            DEFINES
                "lensProjectionType_${lensProjectionType}=1;viewProjectionType_${viewProjectionType}=1"
            OUTPUTS
                "qml/Nx/Screens/private/VideoScreen/FisheyeShaderEffect_${unique_shader_suffix}.frag.qsb"
        )
    endforeach()
endforeach()
