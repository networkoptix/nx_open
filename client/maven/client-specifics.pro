TEMPLATE = app
DEFINES += CL_FORCE_LOGO
DEFINES += QT_QTCOLORPICKER_IMPORT
TRANSLATIONS += ${basedir}/resource/common/translations/client_en.ts \
				${basedir}/resource/common/translations/client_ru.ts

CONFIG(debug, debug|release) {
  CONFIG += console
}

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}
}

mac {
  DEFINES += QN_EXPORT=

  PRIVATE_FRAMEWORKS.files = ../resource/arecontvision
  PRIVATE_FRAMEWORKS.path = Contents/MacOS
  QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS

  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f $$PWD/../resource/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision
        
  #  QMAKE_CXXFLAGS += -DAPI_TEST_MAIN
  #  TARGET = consoleapp
}