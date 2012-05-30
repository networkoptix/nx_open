TEMPLATE = app
CONFIG += console

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
  QMAKE_CXXFLAGS += -Zc:wchar_t
  QMAKE_CXXFLAGS -= -Zc:wchar_t-   
}

mac {
  DEFINES += QN_EXPORT=	
}