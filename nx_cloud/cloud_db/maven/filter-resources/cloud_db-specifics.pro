TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

linux {
    QMAKE_CXXFLAGS += -Werror
}
