INCLUDEPATH +=  ${root.dir}/appserver2/src \
                ${root.dir}/client/nx_client_core/src \
                ${root.dir}/common_libs/nx_vms_utils/src \
                ${root.dir}/common_libs/nx_speech_synthesizer/src \
                ${root.dir}/nx_cloud/cloud_db_client/src/include \
                ${root.dir}/nx_cloud/libvms_gateway/src

mac {
    INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers
}

unix: !mac {
    QT += x11extras
}

mac {
    OBJECTIVE_SOURCES += ${basedir}/src/ui/workaround/mac_utils.mm
    LIBS += -lobjc -framework Foundation -framework AudioUnit -framework AppKit
}

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

linux {
    QMAKE_LFLAGS -= -Wl,--no-undefined
}

OTHER_FILES += ${root.dir}/client/nx_client_desktop/src/ui/help/help_topics.i

SOURCES += ${project.build.directory}/client_app_info_impl.cpp
