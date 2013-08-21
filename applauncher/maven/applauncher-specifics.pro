TEMPLATE = app
QT += core gui network xml

CONFIG(debug, debug|release) {
  CONFIG += console
}

!win32 {
  CONFIG += console
}

QMAKE_RPATHDIR = ""

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
}

mac {
  DEFINES += QN_EXPORT= 
}
