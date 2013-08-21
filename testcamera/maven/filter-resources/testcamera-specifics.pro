TEMPLATE = app
CONFIG += console

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
}

mac {
  DEFINES += QN_EXPORT= 
}