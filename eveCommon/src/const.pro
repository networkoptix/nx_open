TEMPLATE = lib
QT *= multimedia network xml
CONFIG += x86 precompile_header %BUILDLIB
CONFIG -= flat

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

CONFIG(debug, debug|release) {
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = ../build/debug
  MOC_DIR = ../build/debug/generated
  UI_DIR = ../build/debug/generated
  RCC_DIR = ../build/debug/generated
}

CONFIG(release, debug|release) {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = ../build/release
  MOC_DIR = ../build/release/generated
  UI_DIR = ../build/release/generated
  RCC_DIR = ../build/release/generated
}

INCLUDEPATH += $$PWD/common $$PWD/common/core $$PWD/common/utils $$PWD/common/plugins

CONFIG(debug, debug|release) {
  INCLUDEPATH += $$FFMPEG-debug/include
  LIBS += -L$$FFMPEG-debug/bin -L$$FFMPEG-debug/lib
}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
  LIBS += -L$$FFMPEG-release/bin -L$$FFMPEG-release/lib
}

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale

win32 {
  INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
  LIBS += -lws2_32 -lIphlpapi -lOle32
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR
    DEFINES += _CRT_SECURE_NO_WARNINGS
    INCLUDEPATH += $$PWD/../contrib/ffmpeg-misc-headers-win32
  }

  !staticlib {
    DEFINES += QN_EXPORT=Q_DECL_EXPORT
  }

  staticlib {
    DEFINES += QN_EXPORT=
  }
}

mac {
  LIBS += -framework SystemConfiguration
  LIBS += -lz -lbz2
  DEFINES += QN_EXPORT=
}

DEFINES += __STDC_CONSTANT_MACROS


# Clone ssh://hg@vigasin.com/evetools to the same diectory netoptix_vms is located
win32 {
    EVETOOLS_DIR=$$PWD/../../../evetools/win32
    QMAKE_CXXFLAGS += -Zc:wchar_t
    LIBS += -lxerces-c_3
}

mac {
    EVETOOLS_DIR=$$PWD/../../../evetools/mac
    LIBS += -lxerces-c-3.1
}


QMAKE_CXXFLAGS += -I$$EVETOOLS_DIR/include 
LIBS += -L$$EVETOOLS_DIR/lib

XSD_FILES = $$PWD/api/xsd/cameras.xsd \
            $$PWD/api/xsd/layouts.xsd \
            $$PWD/api/xsd/users.xsd \
            $$PWD/api/xsd/resourceTypes.xsd \
            $$PWD/api/xsd/resources.xsd \
            $$PWD/api/xsd/resourcesEx.xsd \
            $$PWD/api/xsd/servers.xsd \
            $$PWD/api/xsd/events.xsd

xsd.name = Generating code from ${QMAKE_FILE_IN}
xsd.input = XSD_FILES
xsd.output = $${MOC_DIR}/xsd_${QMAKE_FILE_BASE}.cpp
xsd.commands = $$EVETOOLS_DIR/bin/xsd cxx-tree --generate-serialization --output-dir $${MOC_DIR} --cxx-regex \"/^(.*).xsd/xsd_\\1.cpp/\" --hxx-regex \"/^(.*).xsd/xsd_\\1.h/\" --generate-ostream --root-element ${QMAKE_FILE_BASE} ${QMAKE_FILE_IN}
xsd.CONFIG += target_predeps
xsd.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += xsd
