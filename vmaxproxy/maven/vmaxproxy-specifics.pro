TEMPLATE = app
CONFIG += console
LIBS -= -lcommon

include(${environment.dir}/qt/custom/qtservice/src/qtservice.pri)
#include(${environment.dir}/qt/custom/qtsingleapplication/src/qtsingleapplication.pri)
include(${environment.dir}/qt/custom/qtsingleapplication/src/qtsinglecoreapplication.pri)

win32 {
  QMAKE_LFLAGS += /MACHINE:x86  
}

mac {
  DEFINES += QN_EXPORT= 
}

CONFIG(debug, debug|release) {
  DESTDIR = ../../build-environment/${arch}/bin/debug/${project.artifactId}
  LIBS -= -L${environment.dir}/qt/bin/x64/debug
  LIBS += -L${environment.dir}/qt/bin/x86/debug  
}

CONFIG(release, debug|release) {
  DESTDIR = ../../build-environment/${arch}/bin/release/${project.artifactId}
  LIBS -= -L${environment.dir}/qt/bin/x64/release
  LIBS += -L${environment.dir}/qt/bin/x86/release  
}