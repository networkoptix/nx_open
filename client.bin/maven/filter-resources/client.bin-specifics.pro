QT += xmlpatterns
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
                ${qt.dir}/include/QtGui/$$QT_VERSION/ \
                ${qt.dir}/include/QtGui/$$QT_VERSION/QtGui/ \
                ${root.dir}/appserver2/src/ \
                ${root.dir}/client.core/src/ \
                ${root.dir}/client/src/ \
                ${root.dir}/common_libs/nx_speach_synthesizer/src/

include($$ADDITIONAL_QT_INCLUDES/qtsingleapplication/src/qtsingleapplication.pri)

mac {
    INCLUDEPATH += /System/Library/Frameworks/OpenAL.framework/Versions/A/Headers/ \
                   ${qt.dir}/lib/QtGui.framework/Headers/$$QT_VERSION/QtGui \
                   ${qt.dir}/lib/QtWidgets.framework/Headers/$$QT_VERSION/QtWidgets
}

unix: !mac {
    QT += x11extras  
}

mac {
    LIBS += -lobjc -framework Foundation -framework AudioUnit -framework AppKit
}

unix:!mac {
    QMAKE_LFLAGS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
}

IS_DYNAMIC_CUSTOMIZATION_ENABLED=${dynamic.customization}
contains( IS_DYNAMIC_CUSTOMIZATION_ENABLED, true ) {
  DEFINES += ENABLE_DYNAMIC_CUSTOMIZATION
}
