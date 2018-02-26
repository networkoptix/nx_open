TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/appserver2/src/
INCLUDEPATH += ${root.dir}/mediaserver_core/src/

LIBS += $$FESTIVAL_LIB

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/ -pie"
}
