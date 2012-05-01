TEMPLATE = app
ICON = eve_logo.icns
CONFIG += console

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
  RC_FILE = ${project.build.sourceDirectory}/${project.artifactId}.rc 
  QMAKE_CXXFLAGS += -Zc:wchar_t
  QMAKE_CXXFLAGS -= -Zc:wchar_t-   
}

mac {
  DEFINES += QN_EXPORT=	
}