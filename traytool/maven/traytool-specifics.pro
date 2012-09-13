TEMPLATE = app

include(${environment.dir}/qt/custom/qtsingleapplication/src/qtsingleapplication.pri)
include(${environment.dir}/qt/custom/qtsingleapplication/src/qtsinglecoreapplication.pri)

CONFIG(debug, debug|release) {
  CONFIG += console
}

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
}

