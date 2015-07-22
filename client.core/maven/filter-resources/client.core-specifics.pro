TEMPLATE = lib

INCLUDEPATH +=  ${qt.dir}/include/QtGui/$$QT_VERSION/ \
                ${qt.dir}/include/QtGui/$$QT_VERSION/QtGui/

mac {
    INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers/ \
                   ${qt.dir}/lib/QtGui.framework/Headers/$$QT_VERSION/QtGui \
}

mac {
    LIBS += -lobjc -framework Foundation -framework AppKit
}

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

