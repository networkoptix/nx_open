TEMPLATE = lib

INCLUDEPATH +=  ${qt.dir}/include/QtGui/$$QT_VERSION/ \
                ${qt.dir}/include/QtGui/$$QT_VERSION/QtGui/ \
                ${root.dir}/appserver2/src/ \
		${root.dir}/nx_cloud/cloud_db_client/src/include/

mac:!ios {
    INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers/ \
                   ${qt.dir}/lib/QtGui.framework/Headers/$$QT_VERSION/QtGui \
}

mac:!ios {
    LIBS += -lobjc -framework Foundation -framework AppKit
}

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}
