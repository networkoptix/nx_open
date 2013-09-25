TEMPLATE = app

DEFINES += CL_FORCE_LOGO
TRANSLATIONS += ${basedir}/translations/client_en.ts \
				${basedir}/translations/client_ru.ts \
				${basedir}/translations/client_zh-CN.ts \
				${basedir}/translations/client_fr.ts \
				${basedir}/translations/client_jp.ts \
				${basedir}/translations/client_ko.ts \
				${basedir}/translations/client_pt-BR.ts \

include(${environment.dir}/qt5/qt-custom/qtsingleapplication/src/qtsingleapplication.pri)

mac {
    INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers/ /usr/X11/include/
}

unix: !mac {
    QT += x11extras  
}
