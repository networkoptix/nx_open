INCLUDEPATH +=  ${root.dir}/appserver2/src \
                ${root.dir}/client/libclient_core/src \
                ${root.dir}/common_libs/nx_vms_utils/src \
                ${root.dir}/client/libclient/src \
                ${root.dir}/common_libs/nx_speach_synthesizer/src

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
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

IS_DYNAMIC_CUSTOMIZATION_ENABLED=${dynamic.customization}
contains( IS_DYNAMIC_CUSTOMIZATION_ENABLED, true ) {
  DEFINES += ENABLE_DYNAMIC_CUSTOMIZATION
}
