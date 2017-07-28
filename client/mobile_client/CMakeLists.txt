if(IOS)
    set(codesignKeychain "nx_build" CACHE STRING "Keychain name to be used to sign binaries.")
endif()

find_package(Qt5 COMPONENTS WebChannel WebSockets)

set(product.title "${mobile_client.name}")
set(product.display.title "${mobile_client.display.name}")

nx_configure_file(
    maven/filter-resources/version.h
    ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    maven/filter-resources/mobile_client_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB_RECURSE QML_FILES static-resources/qml/*.qml static-resources/qml/*.js)

set(resources)

if(IOS)
    if(NOT mobile_client_ios_launch_screen)
        set(mobile_client_ios_launch_screen "${CMAKE_CURRENT_LIST_DIR}/maven/bin-resources/ios/Launch.xib")
    endif()

    set(ios_sources
        src/nx/mobile_client/settings/settings_migration_ios.mm
        src/ui/window_utils_ios.mm
        src/utils/app_delegate.mm
    )

    set(icons_dir "${customization_dir}/icons/ios/ios/images")
    file(GLOB resources "${icons_dir}/icon_*.png" "${icons_dir}/logo*.png")

    list(APPEND resources ${mobile_client_ios_launch_screen})
    list(APPEND resources "${CMAKE_CURRENT_LIST_DIR}/ios/qt.conf")
else()
    set(ios_sources)
endif()

set(target_args
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/mobile_client_app_info_impl.cpp"
        ${ios_sources}
    ADDITIONAL_RESOURCES
        "${customization_dir}/mobile_client/resources"
    OTHER_SOURCES
        ${QML_FILES}
        ${resources}
    PUBLIC_LIBS
        Qt5::WebChannel Qt5::WebSockets
        qtsingleguiapplication
        nx_kit
        nx_client_core)

if(ANDROID)
    nx_add_target(mobile_client LIBRARY LIBRARY_TYPE SHARED ${target_args})
else()
    nx_add_target(mobile_client EXECUTABLE ${target_args})
endif()

if(ANDROID)
    target_link_libraries(mobile_client PUBLIC GLESv2 z log)
endif()

if(box STREQUAL "bpi")
    target_link_libraries(mobile_client PRIVATE GLESv2)
endif()

if(ANDROID)
    set(project.version.name "${mobileClientVersion}")
    set(project.version.code "${buildNumber}")

    file(COPY "maven/bin-resources/android" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    file(COPY "${customization_dir}/icons/android/android"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    configure_file("maven/filter-resources/android/AndroidManifest.xml"
        "${CMAKE_CURRENT_BINARY_DIR}/android/AndroidManifest.xml")
    configure_file("./maven/filter-resources/android/res/values/strings.xml"
        "${CMAKE_CURRENT_BINARY_DIR}/android/res/values/strings.xml")

    get_target_property(OPENSSL_LIBS openssl INTERFACE_LINK_LIBRARIES)
    get_target_property(FFMPEG_LIBS ffmpeg INTERFACE_LINK_LIBRARIES)
    get_target_property(OPENAL_LIBS openal INTERFACE_LINK_LIBRARIES)

    add_android_apk(mobile_client.apk TARGET mobile_client
        APK_NAME "${mobile_client_distribution_name}.apk"
        PACKAGE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/android"
        QML_ROOT_PATH "${CMAKE_CURRENT_LIST_DIR}/static-resources/qml"
        EXTRA_LIBS ${OPENSSL_LIBS} ${FFMPEG_LIBS} ${OPENAL_LIBS}
        KEYSTORE_FILE "${customization_dir}/mobile_client/key.store"
        KEYSTORE_ALIAS "hdwitness"
        KEYSTORE_PASSWORD "hYCmvTDu"
        KEYSTORE_KEY_PASSWORD "31O2zNNy"
    )
endif()

if(IOS)
    set(provisioning_profiles_dir "${static_customization_dir}/mobile_client/ios/provisioning_profiles")

    if(beta)
        configure_file("maven/filter-resources/mobile_client-beta.entitlements"
            "${CMAKE_CURRENT_BINARY_DIR}/mobile_client.entitlements")
        set(provisioning_profile "${provisioning_profiles_dir}/testing.mobileprovision")
    else()
        configure_file("maven/filter-resources/mobile_client-release.entitlements"
            "${CMAKE_CURRENT_BINARY_DIR}/mobile_client.entitlements")
        set(provisioning_profile "${provisioning_profiles_dir}/production.mobileprovision")
    endif()

    execute_process(
        COMMAND "${PROJECT_SOURCE_DIR}/build_utils/shell/get_provisioning_id.sh" "${provisioning_profile}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE provisioning_profile_id
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot get provisioning profile id from \"${provisioning_profile}\"")
    endif()

    set_target_properties(mobile_client PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/maven/filter-resources/Info.plist"
        RESOURCE "${resources}"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${ios.sign.identity}"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_BINARY_DIR}/mobile_client.entitlements"
        XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--keychain ${codesignKeychain}.keychain"
        XCODE_ATTRIBUTE_PROVISIONING_PROFILE "${provisioning_profile_id}"
    )

    set(additional_plugins
        qios
        qtmedia_audioengine
        qtaudio_coreaudio
    )

    if(qml_debug)
        list(APPEND additional_plugins
            qmldbg_debugger
            qmldbg_inspector
            qmldbg_local
            qmldbg_native
            qmldbg_profiler
            qmldbg_server
            qmldbg_tcp
        )
    endif()

    set(static_plugins_args_file "${CMAKE_CURRENT_BINARY_DIR}/static_plugins.parameters")

    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/build_utils/python/qmldeploy.py"
            --print-static-plugins
            --qt-root "${QT_DIR}" --qml-root "${CMAKE_CURRENT_LIST_DIR}/static-resources/qml"
            --additional-plugins ${additional_plugins}
            --output "${static_plugins_args_file}"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot get list of static qml plugins.")
    endif()

    set_property(TARGET mobile_client APPEND_STRING PROPERTY
        LINK_FLAGS " -Wl,-e,_qt_main_wrapper")
    set_property(TARGET mobile_client APPEND_STRING PROPERTY
        LINK_FLAGS " @${static_plugins_args_file}")

    # So far Qt does not export dependent libraries for CMake, so we have to specify them manually.
    target_link_libraries(mobile_client PRIVATE
        "-framework MobileCoreServices"
        "-framework Foundation"
        "-framework UIKit"
        "-framework CoreText"
        "-framework CoreGraphics"
        "-framework MobileCoreServices"
        "-framework Foundation"
        "-framework UIKit"
        "-framework CoreFoundation"
        "-framework Security"
        "-framework SystemConfiguration"
        "-framework OpenGLES"
        "-framework VideoToolbox"
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework AVFoundation"
        "-framework QuartzCore"
        "-framework AssetsLibrary"
        "-L${QT_DIR}/lib"
        qtharfbuzzng qtpcre qtfreetype
        Qt5PlatformSupport
        Qt5LabsControls Qt5LabsTemplates
        Qt5WebView
        z
    )

    set(plugins_import_cpp "${CMAKE_CURRENT_BINARY_DIR}/qt_plugins_import.cpp")
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/build_utils/python/qmldeploy.py"
            --generate-import-cpp
            --qt-root ${QT_DIR} --qml-root "${CMAKE_CURRENT_LIST_DIR}/static-resources/qml"
            --additional-plugins ${additional_plugins}
            --output ${plugins_import_cpp}
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot generate plugins import file.")
    endif()

    set_property(TARGET mobile_client APPEND PROPERTY SOURCES "${plugins_import_cpp}")

    set(app_dir "$<TARGET_FILE_DIR:mobile_client>")

    add_custom_command(TARGET mobile_client PRE_LINK
        COMMAND ${PYTHON_EXECUTABLE} "${PROJECT_SOURCE_DIR}/build_utils/python/qmldeploy.py"
            --qt-root ${QT_DIR} --qml-root "${CMAKE_CURRENT_LIST_DIR}/static-resources/qml"
            --output "${app_dir}/qt_qml"
        COMMENT "Copying QML imports"
    )
endif()
