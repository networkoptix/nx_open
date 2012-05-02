TEMPLATE = app
ICON = traytool.icns

CONFIG(debug, debug|release) {
  CONFIG += console
  PRE_TARGETDEPS += ${basedir}/../common/x86/bin/debug/common.lib
}

CONFIG(release, debug|release) {
  PRE_TARGETDEPS += ${basedir}/../common/x86/bin/release/common.lib
}

win32 {
  RC_FILE = ${project.build.sourceDirectory}/traytool.rc
}