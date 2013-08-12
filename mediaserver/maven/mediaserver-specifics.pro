TEMPLATE = app
CONFIG += console
QT += core network xml sql concurrent multimedia


include(${environment.dir}/qt5/qt-custom/qtservice/src/qtservice.pri)
#include(${environment.dir}/qt5/qt-custom/qtsingleapplication/src/qtsingleapplication.pri)
include(${environment.dir}/qt5/qt-custom/qtsingleapplication/src/qtsinglecoreapplication.pri)

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
}

DEFINES += USE_NX_HTTP
mac {
  DEFINES += QN_EXPORT= 
}
