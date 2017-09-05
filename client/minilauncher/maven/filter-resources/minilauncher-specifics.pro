NAME=${project.artifactId}
TEMPLATE = ${template}
TARGET = ${targetName}
GUID = ${guid}
VERSION = ${release.version}

CONFIG -= flat qt
CONFIG += c++14
RESOURCES += ${project.build.directory}/build/${project.artifactId}.qrc

CONFIG(debug, debug|release) {
  CONFIGURATION=debug
  CONFIG += console
}
else {
  CONFIGURATION=release
}

OUTPUT_PATH = $$clean_path("${libdir}")
DESTDIR = $$OUTPUT_PATH/bin/$$CONFIGURATION/

OBJECTS_DIR = ${project.build.directory}/build/$$CONFIGURATION/
CONFIG += x64
RC_FILE = ${project.build.directory}/hdwitness.rc
ICON = ${customization.dir}/icons/all/favicon.ico
LIBS += shlwapi.lib
QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR /wd4290 /wd4661 /wd4100 /we4717
# /OPT:NOREF is here for a reason, see http://stackoverflow.com/questions/6363991/visual-studio-debug-information-in-release-build.
QMAKE_CXXFLAGS_RELEASE += /Zi /wd4250
QMAKE_LFLAGS_RELEASE += /DEBUG /OPT:NOREF /ENTRY:"mainCRTStartup"
QMAKE_LFLAGS += /MACHINE:${arch} /LARGEADDRESSAWARE

QMAKE_CXXFLAGS_DEBUG += /MTd
QMAKE_CXXFLAGS_RELEASE += /MT
