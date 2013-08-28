TEMPLATE = app

include(${environment.dir}/qt-custom/qtsingleapplication/src/qtsingleapplication.pri)
include(${environment.dir}/qt-custom/qtsingleapplication/src/qtsinglecoreapplication.pri)

TRANSLATIONS += ${basedir}/translations/traytool_en.ts \
				${basedir}/translations/traytool_ru.ts \
				${basedir}/translations/traytool_zh-CN.ts \
				${basedir}/translations/traytool_fr.ts \
				${basedir}/translations/traytool_jp.ts \
				${basedir}/translations/traytool_ko.ts \
				${basedir}/translations/traytool_pt-BR.ts \
