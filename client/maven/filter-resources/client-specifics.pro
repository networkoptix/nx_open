TEMPLATE = app

DEFINES += CL_FORCE_LOGO

INCLUDEPATH +=  ${qt.dir}/include/QtWidgets/$$QT_VERSION/ \
                ${qt.dir}/include/QtWidgets/$$QT_VERSION/QtWidgets/ \
                ${qt.dir}/include/QtGui/$$QT_VERSION/ \
                ${qt.dir}/include/QtGui/$$QT_VERSION/QtGui/ \
                ${root.dir}/appserver2/src/

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
