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

!ios {
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
}
