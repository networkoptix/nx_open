TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/libvms_gateway/src

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}
