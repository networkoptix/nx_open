TEMPLATE = app

include(${environment.dir}/qt-custom/qtservice/src/qtservice.pri)
include(${environment.dir}/qt-custom/qtsingleapplication/src/qtsingleapplication.pri)

QMAKE_RPATHDIR = ""

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
}
