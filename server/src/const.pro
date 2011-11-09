INCLUDEPATH += ../../common/src

win* {
  INCLUDEPATH += ../../common/contrib/ffmpeg-misc-headers-win32
}

QT = core gui network xml opengl multimedia webkit
CONFIG += x86 precompile_header
CONFIG -= flat
TEMPLATE = app
VERSION = 0.0.1
ICON = eve_logo.icns
QMAKE_INFO_PLIST = Info.plist

BUILDLIB = %BUILDLIB
FFMPEG = %FFMPEG
EVETOOLS_DIR = %EVETOOLS_DIR

TARGET = Server

win32: RC_FILE = server.rc

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

CONFIG(debug, debug|release) {
  INCLUDEPATH += $$FFMPEG-debug/include
  LIBS += -L$$FFMPEG-debug/bin -L$$FFMPEG-debug/lib -L$$PWD/../../common/bin/debug -lcommon
}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
  LIBS += -L$$FFMPEG-release/bin -L$$FFMPEG-release/lib -L$$PWD/../../common/bin/release -lcommon
}

win32 {
    QMAKE_CXXFLAGS += -Zc:wchar_t
    LIBS += -lxerces-c_3

    # Define QN_EXPORT only if common build is not static
    isEmpty(BUILDLIB) { DEFINES += QN_EXPORT=Q_DECL_IMPORT }
    !isEmpty(BUILDLIB) { DEFINES += QN_EXPORT= }
}

mac {
    LIBS += -lxerces-c-3.1
    DEFINES += QN_EXPORT=
}

LIBS += -L$$EVETOOLS_DIR/lib

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale

win32 {
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR

    # Don't warn for deprecated 'unsecure' CRT functions.
    DEFINES += _CRT_SECURE_NO_WARNINGS

    # Don't warn for deprecated POSIX functions.
    DEFINES += _CRT_NONSTDC_NO_DEPRECATE 

    INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
    INCLUDEPATH += "$$(DXSDK_DIR)/Include"
    !contains(QMAKE_HOST.arch, x86_64) {
      LIBS += -L"$$(DXSDK_DIR)/Lib/x86"
    } else {
      LIBS += -L"$$(DXSDK_DIR)/Lib/x64"
    }
  }

  LIBS += -lws2_32 -lIphlpapi -lOle32

  LIBS += -ld3d9 -ld3dx9 -lwinmm

  SOURCES += 

  # Define QN_EXPORT only if common build is not static
  isEmpty(BUILDLIB) { DEFINES += QN_EXPORT=Q_DECL_IMPORT }
  !isEmpty(BUILDLIB) { DEFINES += QN_EXPORT= }

  DEFINES += __STDC_CONSTANT_MACROS
}

mac {
  LIBS += -framework IOKit -framework CoreServices
  LIBS += -lz -lbz2

  QMAKE_CXXFLAGS += -msse4.1
  DEFINES += QN_EXPORT=
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

RESOURCES += 
FORMS += 

DEFINES += CL_TRIAL_MODE CL_FORCE_LOGO
#DEFINES += CL_DEFAULT_SKIN_PREFIX=\\\"./trinity\\\"


# Define override specifier.
OVERRIDE_DEFINITION = "override="
win32-msvc*:OVERRIDE_DEFINITION = "override=override"
DEFINES += $$OVERRIDE_DEFINITION
