INCLUDEPATH +=  ${root.dir}/appserver2/src \
                ${root.dir}/client/nx_client_core/src \
                ${root.dir}/common_libs/nx_vms_utils/src \
                ${root.dir}/client/nx_client_desktop/src \
                ${root.dir}/common_libs/nx_speech_synthesizer/src

LIBS += $$FESTIVAL_LIB

mac {
    INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers
}

unix: !mac {
    QT += x11extras
}

mac {
    LIBS += -lobjc -framework Foundation -framework AudioUnit -framework AppKit
}

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/ -pie"
}

IS_DYNAMIC_CUSTOMIZATION_ENABLED=${dynamic.customization}
contains( IS_DYNAMIC_CUSTOMIZATION_ENABLED, true ) {
  DEFINES += ENABLE_DYNAMIC_CUSTOMIZATION
}
