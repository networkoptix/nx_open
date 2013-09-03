TEMPLATE = app

DEFINES += CL_FORCE_LOGO USE_NX_HTTP
TRANSLATIONS += ${basedir}/translations/client_en.ts \
				${basedir}/translations/client_ru.ts \
				${basedir}/translations/client_zh-CN.ts \
				${basedir}/translations/client_fr.ts \
				${basedir}/translations/client_jp.ts \
				${basedir}/translations/client_ko.ts \
				${basedir}/translations/client_pt-BR.ts \

include(${environment.dir}/qt5/qt-custom/qtsingleapplication/src/qtsingleapplication.pri)

win* {
INCLUDEPATH += ${environment.dir}/qt5/qtbase-${arch}/include/QtWidgets/$$QT_VERSION/ \
               ${environment.dir}/qt5/qtbase-${arch}/include/QtWidgets/$$QT_VERSION/QtWidgets/
}


mac {
  INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers/ /usr/X11/include/
}

unix: !mac {
  LIBS += -lX11 -lXfixes -lGL -lGLU
  QT += x11extras  
}
