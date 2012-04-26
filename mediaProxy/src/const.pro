QT = gui core network 
CONFIG += precompile_header
CONFIG -= flat app_bundle

win32 {
  CONFIG += x86
  LIBS += -lws2_32
}

TEMPLATE = app
VERSION = 1.0.0
ICON = mediaproxy.icns

TARGET = mediaproxy

include(../contrib/qtservice/src/qtservice.pri)
include(../../common/contrib/qtsingleapplication/src/qtsinglecoreapplication.pri)


BUILDLIB = %BUILDLIB

include(../../common/contrib/qtsingleapplication/src/qtsingleapplication.pri)

INCLUDEPATH += ../../common/src

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

CONFIG(debug, debug|release) {
  CONFIG += console
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = ../build/debug
  MOC_DIR = ../build/debug/generated
  UI_DIR = ../build/debug/generated
  RCC_DIR = ../build/debug/generated
  win32 {
  PRE_TARGETDEPS += ../../common/bin/debug/common.lib
  }
  unix:!mac {
  PRE_TARGETDEPS += ../../common/bin/debug/libcommon.a
  }
  mac {
  PRE_TARGETDEPS += ../../common/bin/debug/libcommon.dylib
  }
}

CONFIG(release, debug|release) {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = ../build/release
  MOC_DIR = ../build/release/generated
  UI_DIR = ../build/release/generated
  RCC_DIR = ../build/release/generated
  win32 {
  PRE_TARGETDEPS += ../../common/bin/release/common.lib
  }
  unix:!mac {
  PRE_TARGETDEPS += ../../common/bin/release/libcommon.so
  }
  mac {
  PRE_TARGETDEPS += ../../common/bin/release/libcommon.dylib
  }
}

win32: RC_FILE = mediaproxy.rc

win32 {
    # Define QN_EXPORT only if common build is not static
    isEmpty(BUILDLIB) { DEFINES += QN_EXPORT=Q_DECL_IMPORT }
    !isEmpty(BUILDLIB) { DEFINES += QN_EXPORT= }
}

unix {
  LIBS += -lcrypto -lz
  QMAKE_CXXFLAGS += -msse4.1
  DEFINES += QN_EXPORT=
}

DEFINES += __STDC_CONSTANT_MACROS

mac {
  LIBS += -framework IOKit -framework CoreServices
  LIBS += -lbz2

  PRIVATE_FRAMEWORKS.files = ../resource/arecontvision
  PRIVATE_FRAMEWORKS.path = Contents/MacOS
  QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS

  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f $$PWD/../resource/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision

#  QMAKE_CXXFLAGS += -DAPI_TEST_MAIN
#  TARGET = consoleapp
#  CONFIG   += console
#  CONFIG   -= app_bundle
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

# Define override specifier.
OVERRIDE_DEFINITION = "override="
win32-msvc*:OVERRIDE_DEFINITION = "override=override"
DEFINES += $$OVERRIDE_DEFINITION

CONFIG(debug, debug|release) {
  LIBS += -L$$PWD/../../common/bin/debug -lcommon
}
CONFIG(release, debug|release) {
  LIBS += -L$$PWD/../../common/bin/release -lcommon
}

