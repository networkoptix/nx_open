TEMPLATE = app
CONFIG += console

win32 {
  QMAKE_LFLAGS += /MACHINE:x86  
}

 LIBS -= -lcommon

CONFIG(debug, debug|release) {
  DESTDIR = ../../build_environment/${arch}/bin/debug/${project.artifactId}
  LIBS -= -L${environment.dir}/qt/bin/x64/debug
  LIBS += -L${environment.dir}/qt/bin/x86/debug  
}

CONFIG(release, debug|release) {
  DESTDIR = ../../build_environment/${arch}/bin/release/${project.artifactId}
  LIBS -= -L${environment.dir}/qt/bin/x64/release
  LIBS += -L${environment.dir}/qt/bin/x86/release  
}