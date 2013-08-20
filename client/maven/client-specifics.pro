TEMPLATE = app
QT += core gui network opengl xml sql widgets

DEFINES += CL_FORCE_LOGO USE_NX_HTTP
TRANSLATIONS += ${basedir}/translations/client_en.ts \
				${basedir}/translations/client_ru.ts \
				${basedir}/translations/client_zh-CN.ts \
				${basedir}/translations/client_fr.ts \
				${basedir}/translations/client_jp.ts \
				${basedir}/translations/client_ko.ts \
				${basedir}/translations/client_pt-BR.ts \

include(${environment.dir}/qt5/qt-custom/qtsingleapplication/src/qtsingleapplication.pri)
#include(${environment.dir}/qt5/qt-custom/qtsingleapplication/src/qtsinglecoreapplication.pri)

CONFIG(debug, debug|release) {
  CONFIG += console
}

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch} /LARGEADDRESSAWARE
  INCLUDEPATH +=  ${environment.dir}/qt/include/QtGui
}

mac {
  DEFINES += QN_EXPORT=
  INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers/ /usr/X11/include/
}

unix: !mac {
  LIBS += -lX11
  QT += x11extras  
}

unix: {
  LIBS += -lGL -lGLU
}


