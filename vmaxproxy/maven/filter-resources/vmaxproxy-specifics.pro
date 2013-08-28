TEMPLATE = app

LIBS -= -L${environment.dir}/qt/bin/x64/$$CONFIGURATION
LIBS += -L${environment.dir}/qt/bin/x86/$$CONFIGURATION
DESTDIR = ${libdir}/${arch}/bin/$$CONFIGURATION/${project.artifactId}

win* {
    QMAKE_LFLAGS -= /MACHINE:x64
    QMAKE_LFLAGS += /MACHINE:x86
}    