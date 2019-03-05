option(buildApk
    "Build Android APK automatically. Uncheck if you want to use Qt Creator to debug the application."
    ON)

find_program(ANDROIDDEPLOYQT_EXECUTABLE androiddeployqt HINTS "${QT_DIR}/bin")

function(find_buildtools sdk_path result_variable)
    set(buildToolsDir "${sdk_path}/build-tools")

    file(GLOB versions RELATIVE "${buildToolsDir}" "${buildToolsDir}/*.*.*")
    list(SORT versions)

    if(NOT versions)
        message(FATAL_ERROR "Android Build Tools are not found in ${buildToolsDir}.")
    endif()

    list(GET versions -1 version)
    set(${result_variable} "${version}" PARENT_SCOPE)
endfunction()

function(add_android_apk target)
    set(options)
    set(oneValueArgs TARGET FILE_NAME QML_ROOT_PATH VERSION
        KEYSTORE_FILE KEYSTORE_ALIAS KEYSTORE_PASSWORD KEYSTORE_KEY_PASSWORD)
    set(multiValueArgs PACKAGE_SOURCES QML_IMPORT_PATHS EXTRA_LIBS)

    cmake_parse_arguments(APK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT TARGET ${APK_TARGET})
        message(FATAL_ERROR "Target ${APK_TARGET} does not exist.")
    endif()

    string(REPLACE ";" "," extra_libs "${APK_EXTRA_LIBS}")

    find_buildtools("${android_sdk_directory}" buildtools_version)

    if(APK_QML_IMPORT_PATHS)
        set(qml_import_paths "    \"qml-import-paths\": \"${APK_QML_IMPORT_PATHS}\",\n")
    else()
        set(qml_import_paths)
    endif()

    string(STRIP ${CMAKE_CXX_STANDARD_LIBRARIES} cpp_libs)
    string(REPLACE "\"" "" cpp_libs ${cpp_libs})
    string(REPLACE " " ";" cpp_libs ${cpp_libs})
    if(NOT cpp_libs)
        message(FATAL_ERROR "Cannot determine path to C++ standard library.")
    endif()
    list(GET cpp_libs 0 stdcpp_path)
    if(stdcpp_path MATCHES "\.so$")
        set(stdcpp_path  "    \"stdcpp-path\": \"${stdcpp_path}\",\n")
    endif()

    set(PACKAGE_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${target}_apk_template)

    set(settings
        "{\n"
        "    \"description\": \"This file is generated and should not be modified by hand.\",\n"
        "    \"qt\": \"${QT_DIR}\",\n"
        "    \"sdk\": \"${android_sdk_directory}\",\n"
        "    \"sdkBuildToolsRevision\": \"${buildtools_version}\",\n"
        "    \"ndk\": \"${CMAKE_ANDROID_NDK}\",\n"
        "    \"toolchain-prefix\": \"${CMAKE_CXX_ANDROID_TOOLCHAIN_MACHINE}\",\n"
        "    \"tool-prefix\": \"${CMAKE_CXX_ANDROID_TOOLCHAIN_MACHINE}\",\n"
        "    \"toolchain-version\": \"4.9\",\n"
        "    \"ndk-host\": \"${CMAKE_ANDROID_NDK_TOOLCHAIN_HOST_TAG}\",\n"
        "    \"target-architecture\": \"${CMAKE_ANDROID_ARCH_ABI}\",\n"
        "    \"android-package-source-directory\": \"${PACKAGE_SOURCE}\",\n"
        "    \"android-extra-libs\": \"${extra_libs}\",\n"
        ${stdcpp_path}
        "    \"qml-root-path\": \"${APK_QML_ROOT_PATH}\",\n"
        ${qml_import_paths}
        "    \"application-binary\": \"$<TARGET_FILE:${APK_TARGET}>\"\n"
        "}\n"
    )

    string(CONCAT settings ${settings})

    set(settings_file "${CMAKE_CURRENT_BINARY_DIR}/android-deployment-settings.json")
    file(GENERATE OUTPUT "${settings_file}" CONTENT "${settings}")
    nx_store_known_file(${settings_file})

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(build_type --release)
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
        set(build_type)
        set(apk_suffix "debug")
    endif()

    set(apk_dir "${CMAKE_CURRENT_BINARY_DIR}/${APK_TARGET}_apk")

    add_custom_command(
        OUTPUT ${APK_FILE_NAME}
        DEPENDS ${APK_TARGET}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${PACKAGE_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${APK_PACKAGE_SOURCES} ${PACKAGE_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${apk_dir}/libs/${CMAKE_ANDROID_ARCH_ABI}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${APK_TARGET}>"
            "${apk_dir}/libs/${CMAKE_ANDROID_ARCH_ABI}"
        COMMAND ${ANDROIDDEPLOYQT_EXECUTABLE} ${build_type} ${sign_parameters}
            --input "${settings_file}" --output "${apk_dir}" --gradle --verbose
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${apk_dir}/build/outputs/apk/${APK_TARGET}_apk-${apk_suffix}.apk"
            ${APK_FILE_NAME}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${PACKAGE_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${apk_dir}
    )

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
