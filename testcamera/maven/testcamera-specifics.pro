TEMPLATE = app
CONFIG += console

DEFINES += USE_NX_HTTP

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
}

mac {
  DEFINES += QN_EXPORT= 
}
