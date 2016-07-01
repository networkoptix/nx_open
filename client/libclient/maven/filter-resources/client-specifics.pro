INCLUDEPATH +=  ${root.dir}/appserver2/src \
                ${root.dir}/client/libclient_core/src \
                ${root.dir}/common_libs/nx_vms_utils/src \
                ${root.dir}/common_libs/nx_speach_synthesizer/src \
                ${root.dir}/nx_cloud/cloud_db_client/src/include

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

OTHER_FILES += ${root.dir}/client/src/ui/help/help_topics.i

SOURCES += ${project.build.directory}/client_app_info_impl.cpp
