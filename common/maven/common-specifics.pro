TEMPLATE = lib
QT += core gui network xml sql

win32 {
  pb.commands = ${libdir}/bin/protoc --proto_path=${project.build.sourceDirectory}/api/pb --cpp_out=$${MOC_DIR} ${project.build.sourceDirectory}/api/pb/${QMAKE_FILE_BASE}.proto
  LIBS+=winmm.lib
  LIBS += -ldnsapi
}

unix {
  pb.commands = protoc --proto_path=${project.build.sourceDirectory}/api/pb --cpp_out=$${MOC_DIR} ${project.build.sourceDirectory}/api/pb/${QMAKE_FILE_BASE}.proto
}

mac {
  LIBS += -L/usr/X11/lib/
}


DEFINES += USE_NX_HTTP

pb.name = Generating code from ${QMAKE_FILE_IN} to ${QMAKE_FILE_BASE}
pb.input = PB_FILES
pb.output = $${MOC_DIR}/${QMAKE_FILE_BASE}.pb.cc
pb.CONFIG += target_predeps
pb.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += pb

TRANSLATIONS += ${basedir}/translations/common_en.ts \
				${basedir}/translations/common_zh-CN.ts \
				${basedir}/translations/common_fr.ts \
				${basedir}/translations/qt_ru.ts \
				${basedir}/translations/qt_zh-CN.ts \
				${basedir}/translations/qt_fr.ts \
				${basedir}/translations/qt_jp.ts \
				${basedir}/translations/qt_ko.ts \
				${basedir}/translations/qt_pt-BR.ts \

#				${basedir}/translations/common_ru.ts \
#				${basedir}/translations/common_jp.ts \
#				${basedir}/translations/common_ko.ts \
#				${basedir}/translations/common_pt-BR.ts \
                
                
                
                
                
