TEMPLATE = app

include(${environment.dir}/qt-custom/qtsingleapplication/src/qtsingleapplication.pri)
include(${environment.dir}/qt-custom/qtsingleapplication/src/qtsinglecoreapplication.pri)

CONFIG(debug, debug|release) {
  CONFIG += console
}

#LIBS -= -lqjson

win32 {
  QMAKE_LFLAGS += /MACHINE:${arch}  
}

TRANSLATIONS += ${basedir}/translations/traytool_en.ts \
				${basedir}/translations/traytool_ru.ts \
				${basedir}/translations/traytool_zh-CN.ts \
				${basedir}/translations/traytool_fr.ts \
