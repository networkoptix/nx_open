find_package(Qt5 COMPONENTS WebChannel WebSockets)

set(product.title "${mobile_client.name}")
set(product.display.title "${mobile_client.display.name}")

nx_configure_file(
    maven/filter-resources/mobile_client_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB_RECURSE QML_FILES static-resources/qml/*.qml static-resources/qml/*.js)

set(target_args
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/mobile_client_app_info_impl.cpp"
    ADDITIONAL_RESOURCES
        "${customization_dir}/mobile_client/resources"
    OTHER_SOURCES
        ${QML_FILES}
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
    set(project.version.name "${releaseVersion}")
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
        APK_NAME "${client_distribution_name}.apk"
        PACKAGE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/android"
        QML_ROOT_PATH "${CMAKE_CURRENT_LIST_DIR}/static-resources/qml"
        EXTRA_LIBS ${OPENSSL_LIBS} ${FFMPEG_LIBS} ${OPENAL_LIBS}
        KEYSTORE_FILE "${customization_dir}/mobile_client/key.store"
        KEYSTORE_ALIAS "hdwitness"
        KEYSTORE_PASSWORD "hYCmvTDu"
        KEYSTORE_KEY_PASSWORD "31O2zNNy"
    )
endif()

project(mobile_client)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mobile_client)
