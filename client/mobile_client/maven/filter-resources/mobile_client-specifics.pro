TEMPLATE = app


INCLUDEPATH += \
    ${root.dir}/appserver2/src \
    ${root.dir}/client/nx_client_core/src \
    ${root.dir}/common_libs/nx_vms_utils/src \
    ${root.dir}/nx_cloud/cloud_db_client/src/include

unix: !mac {
    LIBS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
    LIBS += "-Wl,-rpath-link,$$OPENSSL_DIR/lib"
    # Ignore missing platform-dependent libs required for libproxydecoder.so
    LIBS += "-Wl,--allow-shlib-undefined"
}

QML_IMPORT_PATH = ${basedir}/static-resources/qml

android {
    QT += androidextras

# TODO: #dklychkov make this not hardcoded
    PRE_TARGETDEPS += \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libcommon.a \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libappserver2.a \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libnx_audio.a \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libnx_media.a \
    
    ANDROID_EXTRA_LIBS += \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libcrypto.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libssl.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libopenal.so

    ANDROID_EXTRA_LIBS += \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libavutil.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libavcodec.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libavformat.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libavdevice.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libswscale.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libswresample.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libavfilter.so

    ANDROID_PACKAGE_SOURCE_DIR = ${basedir}/${arch}/android

    OTHER_FILES += \
        $$ANDROID_PACKAGE_SOURCE_DIR/AndroidManifest.xml
}

ios {
    QMAKE_INFO_PLIST = ${basedir}/${arch}/Info.plist
    OTHER_FILES += $${QMAKE_INFO_PLIST}
    OBJECTIVE_SOURCES += \
        ${basedir}/src/ui/window_utils_ios.mm \
        ${basedir}/src/nx/mobile_client/settings/settings_migration_ios.mm \
        ${basedir}/src/utils/app_delegate.mm

    ios_icon.files = $$files(${basedir}/${arch}/ios/images/icon*.png)
    QMAKE_BUNDLE_DATA += ios_icon

    ios_logo.files = $$files(${basedir}/${arch}/ios/images/logo*.png)
    QMAKE_BUNDLE_DATA += ios_logo

    launch_image.files = $$files(${basedir}/${arch}/ios/Launch.xib)
    QMAKE_BUNDLE_DATA += launch_image

    QMAKE_XCODE_CODE_SIGN_IDENTITY = "${ios.sign.identity}"
    XCODEBUILD_FLAGS += PROVISIONING_PROFILE=${provisioning_profile_id}
    XCODEBUILD_FLAGS += CODE_SIGN_ENTITLEMENTS=${mobile_client_entitlements}
    ${ios.skip.sign}: true {
        XCODEBUILD_FLAGS += CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
    }
}

SOURCES += ${project.build.directory}/mobile_client_app_info_impl.cpp
