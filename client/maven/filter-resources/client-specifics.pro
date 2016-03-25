INCLUDEPATH +=  ${qt.dir}/include/QtWidgets/ \
                ${qt.dir}/include/QtWidgets/$$QT_VERSION/ \
                ${qt.dir}/include/QtWidgets/$$QT_VERSION/QtWidgets/ \
                ${qt.dir}/include/QtGui/ \
                ${qt.dir}/include/QtGui/$$QT_VERSION/ \
                ${qt.dir}/include/QtGui/$$QT_VERSION/QtGui/ \
                ${root.dir}/appserver2/src/ \
                ${root.dir}/client.core/src/ \
                ${root.dir}/nx_cloud/cloud_db_client/src/include/

include($$ADDITIONAL_QT_INCLUDES/qtsingleapplication/src/qtsingleapplication.pri)

mac {
    INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers/ \
                   ${qt.dir}/lib/QtGui.framework/Headers/$$QT_VERSION/QtGui \
                   ${qt.dir}/lib/QtWidgets.framework/Headers/$$QT_VERSION/QtWidgets
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