TEMPLATE = app
CONFIG += console

include(${environment.dir}/qt/custom/qtservice/src/qtservice.pri)
include(${environment.dir}/qt/custom/qtsingleapplication/src/qtsingleapplication.pri)
include(${environment.dir}/qt/custom/qtsingleapplication/src/qtsinglecoreapplication.pri)

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
}

mac {
  DEFINES += QN_EXPORT= 
}