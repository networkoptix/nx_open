TEMPLATE = app

DEFINES += CL_FORCE_LOGO

INCLUDEPATH += \
    ${root.dir}/appserver2/src/ \
    ${root.dir}/client.core/src/ \
	${qt.dir}/include/QtMultimedia/ \
    ${qt.dir}/include/QtMultimedia/$$QT_VERSION/ \
    ${qt.dir}/include/QtMultimedia/$$QT_VERSION/QtMultimedia/ \
	${qt.dir}/include/QtGui/ \
    ${qt.dir}/include/QtGui/$$QT_VERSION/ \
    ${qt.dir}/include/QtGui/$$QT_VERSION/QtGui/ \



unix: !ios {
    LIBS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
    LIBS += "-Wl,-rpath-link,$$OPENSSL_DIR/lib"
}

android {
    QT += androidextras

# TODO: #dklychkov make this not hardcoded
    PRE_TARGETDEPS += \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libcommon.a \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libappserver2.a \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libnx_audio.a \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libnx_media.a \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libnx_streaming.a
    
    ANDROID_EXTRA_LIBS += \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libcrypto.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libssl.so \
        $$OUTPUT_PATH/lib/libopenal.so

    ANDROID_PACKAGE_SOURCE_DIR = ${basedir}/${arch}/android

    OTHER_FILES += \
        $$ANDROID_PACKAGE_SOURCE_DIR/AndroidManifest.xml
}

ios {
    QMAKE_INFO_PLIST = ${basedir}/${arch}/Info.plist
    OTHER_FILES += $${QMAKE_INFO_PLIST}
    OBJECTIVE_SOURCES += \
        ${basedir}/src/ui/window_utils_ios.mm \
        ${basedir}/src/utils/settings_migration_ios.mm

    ios_icon.files = $$files(${basedir}/${arch}/ios/images/icon*.png)
    QMAKE_BUNDLE_DATA += ios_icon

    ios_logo.files = $$files(${basedir}/${arch}/ios/images/logo*.png)
    QMAKE_BUNDLE_DATA += ios_logo

    launch_image.files = $$files(${basedir}/${arch}/ios/Launch.xib)
    QMAKE_BUNDLE_DATA += launch_image

    QMAKE_XCODE_CODE_SIGN_IDENTITY = ${ios.sign.identity}
    XCODEBUILD_FLAGS += PROVISIONING_PROFILE=${provisioning_profile_id}
    XCODEBUILD_FLAGS += CODE_SIGN_ENTITLEMENTS=mobile_client.entitlements
}
