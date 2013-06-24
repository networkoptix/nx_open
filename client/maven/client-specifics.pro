TEMPLATE = app
DEFINES += CL_FORCE_LOGO
DEFINES += QT_QTCOLORPICKER_IMPORT
TRANSLATIONS += ${basedir}/translations/client_en.ts \
				${basedir}/translations/client_ru.ts \
				${basedir}/translations/client_zh-CN.ts \
				${basedir}/translations/client_fr.ts \
				${basedir}/translations/qt_ru.ts \
				${basedir}/translations/qt_zh-CN.ts \
				${basedir}/translations/qt_fr.ts

include(${environment.dir}/qt-custom/qtsingleapplication/src/qtsingleapplication.pri)
#include(${environment.dir}/qt-custom/qtsingleapplication/src/qtsinglecoreapplication.pri)

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

unix {
  LIBS += -lX11
}

