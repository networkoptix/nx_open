TEMPLATE = app

DEFINES += CL_FORCE_LOGO
#TRANSLATIONS += ${basedir}/translations/client_en.ts \

#				${basedir}/translations/client_ru.ts \
#				${basedir}/translations/client_zh-CN.ts \
#				${basedir}/translations/client_fr.ts \
#				${basedir}/translations/client_jp.ts \
#				${basedir}/translations/client_ko.ts \
#				${basedir}/translations/client_pt-BR.ts \

INCLUDEPATH += \
    ${root.dir}/appserver2/src/ \
    ${root.dir}/client.core/src/

unix: !ios {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

android {
    QT += androidextras

# TODO: #dklychkov make this not hardcoded
    PRE_TARGETDEPS += \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libcommon.a \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libappserver2.a
    
    ANDROID_EXTRA_LIBS += \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libcrypto.so \
        $$OUTPUT_PATH/lib/$$CONFIGURATION/libssl.so

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
    XCODEBUILD_FLAGS += CODE_SIGN_ENTITLEMENTS=${mobile_client_entitlements}
    eval (${mac.skip.sign} = true) {
        XCODEBUILD_FLAGS += CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
    }
}
