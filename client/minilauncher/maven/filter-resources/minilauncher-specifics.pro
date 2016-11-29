cache()

NAME=minilauncher
TEMPLATE = app
TARGET = minilauncher
GUID = {d4a489d6-017e-432f-afd8-cfa7645e5256}
VERSION = 3.0.0

CONFIG -= flat qt
CONFIG += c++14 console
RESOURCES += C:\\develop\\2nx_vms\\client\\minilauncher\\x64/build/minilauncher.qrc

CONFIG(debug, debug|release) {
  CONFIGURATION=debug
}
else {
  CONFIGURATION=release
}

OUTPUT_PATH = $$clean_path("C:\\develop\\2nx_vms\\client\\minilauncher/../../build_environment/target")
DESTDIR = $$OUTPUT_PATH/bin/$$CONFIGURATION/

OBJECTS_DIR = C:\\develop\\2nx_vms\\client\\minilauncher\\x64/build/$$CONFIGURATION/
CONFIG += x64
RC_FILE = C:\\develop\\2nx_vms\\client\\minilauncher\\x64/hdwitness.rc
ICON = C:\\develop\\2nx_vms\\client\\minilauncher/../../customization/default/icons/favicon.ico
LIBS += shlwapi.lib
# Note on /bigobj: http://stackoverflow.com/questions/15110580/penalty-of-the-msvs-linker-flag-bigobj
QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR /bigobj /wd4290 /wd4661 /wd4100 /we4717
# /OPT:NOREF is here for a reason, see http://stackoverflow.com/questions/6363991/visual-studio-debug-information-in-release-build.
QMAKE_CXXFLAGS_RELEASE += /Zi /wd4250
QMAKE_LFLAGS_RELEASE += /DEBUG /OPT:NOREF
QMAKE_LFLAGS += /MACHINE:x64 /LARGEADDRESSAWARE

QMAKE_CXXFLAGS_DEBUG += /MTd
QMAKE_CXXFLAGS_RELEASE += /MT