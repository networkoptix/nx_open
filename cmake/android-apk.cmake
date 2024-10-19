## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

option(buildApk
    "Build Android APK automatically. Uncheck if you want to use Qt Creator to debug the application."
    ON)

function(find_buildtools sdk_path result_variable)
    set(buildToolsDir "${sdk_path}/build-tools")

    file(GLOB versions RELATIVE "${buildToolsDir}" CONFIGURE_DEPENDS "${buildToolsDir}/*.*.*")
    list(SORT versions)

    if(NOT versions)
        message(FATAL_ERROR "Android Build Tools are not found in ${buildToolsDir}.")
    endif()

    list(GET versions -1 version)
    set(${result_variable} "${version}" PARENT_SCOPE)
endfunction()

function(add_android_apk target)
    set(options)
    set(oneValueArgs TARGET FILE_NAME QML_ROOT_PATH VERSION TEMPLATE
        KEYSTORE_FILE KEYSTORE_ALIAS KEYSTORE_PASSWORD KEYSTORE_KEY_PASSWORD
        MIN_SDK_VERSION TARGET_SDK_VERSION GRADLE_DEPENDENCIES DEPENDS)
    set(multiValueArgs QML_IMPORT_PATHS EXTRA_LIBS EXTRA_JAVA_LIBS)

    cmake_parse_arguments(APK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT TARGET ${APK_TARGET})
        message(FATAL_ERROR "Target ${APK_TARGET} does not exist.")
    endif()

    string(REPLACE ";" "," extra_libs "${APK_EXTRA_LIBS}")

    find_buildtools("$ENV{ANDROID_HOME}" buildtools_version)

    if(APK_QML_IMPORT_PATHS)
        set(qml_import_paths "    \"qml-import-paths\": \"${APK_QML_IMPORT_PATHS}\",\n")
    else()
        set(qml_import_paths)
    endif()

    set(PACKAGE_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${target}_apk_template)
    # This directory is used after the build by the AAB packaging script.
    nx_store_ignored_build_directory(${PACKAGE_SOURCE})

    if(NOT ANDROID_HOST_TAG)
        set(ANDROID_HOST_TAG ${CMAKE_ANDROID_NDK_TOOLCHAIN_HOST_TAG})
    endif()

    if(NOT APK_MIN_SDK_VERSION)
        set(APK_MIN_SDK_VERSION 23)
    endif()
    if(NOT APK_TARGET_SDK_VERSION)
        set(APK_TARGET_SDK_VERSION 34)
    endif()

    set(settings
        "{\n"
        "    \"description\": \"This file is generated and should not be modified by hand.\",\n"
        "    \"qt\": \"${QT_DIR}\",\n"
        "    \"sdk\": \"$ENV{ANDROID_HOME}\",\n"
        "    \"sdkBuildToolsRevision\": \"${buildtools_version}\",\n"
        "    \"android-min-sdk-version\": \"${APK_MIN_SDK_VERSION}\",\n"
        "    \"android-target-sdk-version\": \"${APK_TARGET_SDK_VERSION}\",\n"
        "    \"ndk\": \"${CMAKE_ANDROID_NDK}\",\n"
        "    \"toolchain-prefix\": \"llvm\",\n"
        "    \"tool-prefix\": \"llvm\",\n"
        "    \"useLLVM\": true,\n"
        "    \"toolchain-version\": \"${CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION}\",\n"
        "    \"ndk-host\": \"${ANDROID_HOST_TAG}\",\n"
        "    \"architectures\": {\"${CMAKE_ANDROID_ARCH_ABI}\": \"${CMAKE_LIBRARY_ARCHITECTURE}\"},\n"
        "    \"android-package-source-directory\": \"${PACKAGE_SOURCE}\",\n"
        "    \"android-extra-libs\": \"${extra_libs}\",\n"
        "    \"stdcpp-path\": \"${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/sysroot/usr/lib\",\n"
        "    \"qml-root-path\": \"${APK_QML_ROOT_PATH}\",\n"
        ${qml_import_paths}
        "    \"qml-importscanner-binary\": \"${QT_HOST_PATH}/${QT6_HOST_INFO_LIBEXECDIR}/qmlimportscanner\",\n"
        "    \"rcc-binary\": \"${QT_HOST_PATH}/${QT6_HOST_INFO_LIBEXECDIR}/rcc\",\n"
        "    \"application-binary\": \"${APK_TARGET}\"\n"
        "}\n"
    )

    string(CONCAT settings ${settings})

    set(settings_file "${CMAKE_CURRENT_BINARY_DIR}/android-deployment-settings.json")
    file(GENERATE OUTPUT "${settings_file}" CONTENT "${settings}")
    nx_store_known_file(${settings_file})

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(build_type "release")
        set(build_type_arg "--release")
        set(apk_suffix "release")

        if(APK_KEYSTORE_FILE AND APK_KEYSTORE_ALIAS
            AND APK_KEYSTORE_PASSWORD AND APK_KEYSTORE_KEY_PASSWORD)

            set(sign_parameters
                --sign "${APK_KEYSTORE_FILE}" "${APK_KEYSTORE_ALIAS}"
                --storepass "${APK_KEYSTORE_PASSWORD}"
                --keypass "${APK_KEYSTORE_KEY_PASSWORD}")
            set(apk_suffix "${apk_suffix}-signed")
        else()
            set(sign_parameters)
            set(apk_suffix "${apk_suffix}-unsigned")
        endif()
    else()
        set(build_type "debug")
        set(build_type_arg)
        set(apk_suffix "debug")
    endif()

    set(apk_dir "${CMAKE_CURRENT_BINARY_DIR}/${APK_TARGET}_apk")
    set(libs_dir "${apk_dir}/libs")

    string(REPLACE ";" "\n" java_libs "${APK_EXTRA_JAVA_LIBS}\n")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/extra_java_libs.txt "${java_libs}")
    nx_store_known_file(${CMAKE_CURRENT_BINARY_DIR}/extra_java_libs.txt)

    file(COPY ${QT_DIR}/src/android/templates/build.gradle DESTINATION ${APK_TEMPLATE})

    file(READ ${APK_TEMPLATE}/build.gradle FILE_CONTENTS)

    if(APK_GRADLE_DEPENDENCIES)
        string(REGEX REPLACE
            "(\ndependencies {\n)"
            "\\1${APK_GRADLE_DEPENDENCIES}\n"
            FILE_CONTENTS "${FILE_CONTENTS}")
    endif()

    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/build.gradle "${FILE_CONTENTS}")
    nx_store_known_file(${CMAKE_CURRENT_BINARY_DIR}/build.gradle)

    if(APK_EXTRA_JAVA_LIBS)
        set(copy_java_libs_command COMMAND ${CMAKE_COMMAND} -E copy ${APK_EXTRA_JAVA_LIBS} ${libs_dir})
    else()
        set(copy_java_libs_command)
    endif()

    add_custom_command(
        OUTPUT ${APK_FILE_NAME}
        DEPENDS ${APK_TARGET} ${APK_DEPENDS}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${PACKAGE_SOURCE} ${apk_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${APK_TEMPLATE} ${PACKAGE_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/build.gradle ${PACKAGE_SOURCE}/build.gradle
        COMMAND ${CMAKE_COMMAND} -E make_directory "${libs_dir}/${CMAKE_ANDROID_ARCH_ABI}"
        ${copy_java_libs_command}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${APK_TARGET}>"
            "${apk_dir}/libs/${CMAKE_ANDROID_ARCH_ABI}/lib${APK_TARGET}_${CMAKE_ANDROID_ARCH_ABI}.so"
        COMMAND ${CMAKE_COMMAND} -E env
            JAVA_HOME=${CONAN_OPENJDK_ROOT}
            --modify PATH=path_list_prepend:${CONAN_OPENJDK_ROOT}/bin
            $<TARGET_FILE:Qt6::androiddeployqt> ${build_type_arg} ${sign_parameters}
            --input ${settings_file} --output ${apk_dir} --gradle --verbose
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${apk_dir}/build/outputs/apk/${build_type}/${APK_TARGET}_apk-${apk_suffix}.apk"
            ${APK_FILE_NAME}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${apk_dir}
    )
    # The contents of PACKAGE_SOURCE will be used by AAB build build script. We need to keep it.

    set(target_args)
    if(buildApk)
        set(target_args ALL)
    endif()

    add_custom_target(${target} ${target_args} DEPENDS ${APK_FILE_NAME})

    string(REPLACE ";" " " extra_libs "${APK_EXTRA_LIBS}")

    set(pro_file_content
        "#This file is generated and should not be modified by hand.\n"
        "TEMPLATE = app\n"
        "TARGET = ${APK_TARGET}\n"
        "VERSION = ${APK_VERSION}\n"
        "CONFIG(debug, debug|release): CONFIG += qml_debug\n"
        "QML_IMPORT_PATH = ${APK_QML_IMPORT_PATHS}\n"
        "QML_ROOT_PATH = ${APK_QML_ROOT_PATH}\n"
        "ANDROID_EXTRA_LIBS += ${extra_libs}\n"
        "DESTDIR = $<TARGET_FILE_DIR:${APK_TARGET}>\n"
        "ANDROID_PACKAGE_SOURCE_DIR = $$_PRO_FILE_PWD_/android\n"
        "OTHER_FILES += $$ANDROID_PACKAGE_SOURCE_DIR/AndroidManifest.xml\n"
    )
    string(CONCAT pro_file_content ${pro_file_content})

    set(pro_file "${CMAKE_CURRENT_BINARY_DIR}/${APK_TARGET}.pro")
    file(GENERATE OUTPUT "${pro_file}" CONTENT "${pro_file_content}")
    nx_store_known_file(${pro_file})
endfunction()
