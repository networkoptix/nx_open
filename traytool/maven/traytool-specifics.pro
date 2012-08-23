TEMPLATE = app

include(${environment.dir}/qt/custom/qtsingleapplication/src/qtsingleapplication.pri)
include(${environment.dir}/qt/custom/qtsingleapplication/src/qtsinglecoreapplication.pri)

CONFIG(debug, debug|release) {
  CONFIG += console
  PRE_TARGETDEPS += ${basedir}/../common/x86/bin/debug/common.lib
}

CONFIG(release, debug|release) {
  PRE_TARGETDEPS += ${basedir}/../common/x86/bin/release/common.lib
}

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
}