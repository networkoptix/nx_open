TEMPLATE = app

DEFINES += CL_FORCE_LOGO
TRANSLATIONS += ${basedir}/translations/client_en.ts \

#				${basedir}/translations/client_ru.ts \
#				${basedir}/translations/client_zh-CN.ts \
#				${basedir}/translations/client_fr.ts \
#				${basedir}/translations/client_jp.ts \
#				${basedir}/translations/client_ko.ts \
#				${basedir}/translations/client_pt-BR.ts \

INCLUDEPATH +=  ${qt.dir}/include/QtWidgets/$$QT_VERSION/ \
                ${qt.dir}/include/QtWidgets/$$QT_VERSION/QtWidgets/ \
                ${root.dir}/appserver2/src/

include($$ADDITIONAL_QT_INCLUDES/qtsingleapplication/src/qtsingleapplication.pri)

mac {
    INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers/
}

unix: !mac {
    QT += x11extras  
}

mac {
    OBJECTIVE_SOURCES += ${basedir}/src/ui/workaround/mac_utils.mm
    LIBS += -lobjc -framework Foundation -framework AudioUnit -framework AppKit
}

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}
