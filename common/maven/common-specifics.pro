TEMPLATE = lib

win32 {
  pb.commands = ${project.build.directory}/build/bin/protoc --proto_path=${project.build.sourceDirectory}/api/pb --cpp_out=$${MOC_DIR} ${project.build.sourceDirectory}/api/pb/${QMAKE_FILE_BASE}.proto
}

unix {
  pb.commands = protoc --proto_path=${project.build.sourceDirectory}/api/pb --cpp_out=$${MOC_DIR} ${project.build.sourceDirectory}/api/pb/${QMAKE_FILE_BASE}.proto
}

pb.name = Generating code from ${QMAKE_FILE_IN} to ${QMAKE_FILE_BASE}
pb.input = PB_FILES
pb.output = $${MOC_DIR}/${QMAKE_FILE_BASE}.pb.cc
pb.CONFIG += target_predeps
pb.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += pb

#TRANSLATIONS += ${project.build.sourceDirectory}/translations/common_en.ts \
#				${project.build.sourceDirectory}/translations/common_ru.ts