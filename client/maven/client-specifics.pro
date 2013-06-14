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
  QMAKE_LFLAGS += /MACHINE:${arch} /LARGEADDRESSAWARE
}

mac {
  DEFINES += QN_EXPORT=
  INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers/ /usr/X11/include/
}

unix: !mac {
  LIBS += -lX11
}
