LIBS -= -L${qt.dir}/lib
LIBS += -L${qt.dir}/../qtbase-x86/lib
DESTDIR = ${libdir}/${arch}/bin/$$CONFIGURATION/${project.artifactId}

win* {
    QMAKE_LFLAGS -= /MACHINE:x64
    QMAKE_LFLAGS += /MACHINE:x86
}    