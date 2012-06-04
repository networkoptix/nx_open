QT = core gui network xml opengl webkit
CONFIG += console precompile_header
CONFIG -= flat app_bundle

win32 {
  QT += multimedia 
  CONFIG += x86
}

TEMPLATE = app
VERSION = 0.0.1
ICON = eve_logo.icns
QMAKE_INFO_PLIST = Info.plist

BUILDLIB = %BUILDLIB
FFMPEG = %FFMPEG
EVETOOLS_DIR = %EVETOOLS_DIR

TARGET = mediaserver

include(../contrib/qtservice/src/qtservice.pri)
include(../../common/contrib/qtsingleapplication/src/qtsinglecoreapplication.pri)

win* {
    INCLUDEPATH += $$PWD/../../common/contrib/openssl/include
    INCLUDEPATH += ../../common/contrib/ffmpeg-misc-headers-win32
    
    win32-msvc2010 {
        LIBS += -L$$PWD/../../common/contrib/openssl/bin/win32-msvc2010
    }
    win32-msvc2008 {
        LIBS += -L$$PWD/../../common/contrib/openssl/bin
    }
    LIBS += -llibeay32
}

INCLUDEPATH += ../../common/src
INCLUDEPATH += ../../common/contrib/qjson/include
INCLUDEPATH += ../../common/contrib/qt

win32: RC_FILE = server.rc

CONFIG(debug, debug|release) {
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
  PRE_TARGETDEPS += ../../common/bin/release/libcommon.a
  }
  mac {
  PRE_TARGETDEPS += ../../common/bin/release/libcommon.dylib
  }
}

CONFIG(debug, debug|release) {
  INCLUDEPATH += $$FFMPEG-debug/include
  LIBS += -L$$FFMPEG-debug/bin -L$$FFMPEG-debug/lib -L$$PWD/../../common/bin/debug -lcommon -L$$EVETOOLS_DIR/lib/debug

  win32 {
    LIBS += -L../../common/contrib/qjson/lib/win32/debug 
  }

}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
  LIBS += -L$$FFMPEG-release/bin -L$$FFMPEG-release/lib -L$$PWD/../../common/bin/release -lcommon -L$$EVETOOLS_DIR/lib/release

  win32 {
    LIBS += -L../../common/contrib/qjson/lib/win32/release 
  }
}

QMAKE_CXXFLAGS += -I$$EVETOOLS_DIR/include

win32 {
    QMAKE_CXXFLAGS += -Zc:wchar_t
    QMAKE_CXXFLAGS -= -Zc:wchar_t-

    LIBS += -llibprotobuf

    # Define QN_EXPORT only if common build is not static
    isEmpty(BUILDLIB) { DEFINES += QN_EXPORT=Q_DECL_IMPORT }
    !isEmpty(BUILDLIB) { DEFINES += QN_EXPORT= }
}

mac {
    LIBS += -L../../common/contrib/qjson/lib/mac -lprotobuf
    DEFINES += QN_EXPORT=
}

unix:!mac {
    LIBS += -lprotobuf
    HARDWARE_PLATFORM = $$system(uname -i)
    contains( HARDWARE_PLATFORM, x86_64 ) {
        LIBS += -L../../common/contrib/qjson/lib/linux-64 
    } else {
        LIBS += -L../../common/contrib/qjson/lib/linux-32
    }
}

LIBS += -L$$EVETOOLS_DIR/lib

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale -lqjson

win32 {
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR

    # Don't warn for deprecated 'unsecure' CRT functions.
    DEFINES += _CRT_SECURE_NO_WARNINGS

    # Don't warn for deprecated POSIX functions.
    DEFINES += _CRT_NONSTDC_NO_DEPRECATE 

    INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
  }

  LIBS += -lws2_32 -lIphlpapi -lOle32

  LIBS += -lwinmm

  SOURCES += 

  # Define QN_EXPORT only if common build is not static
  isEmpty(BUILDLIB) { DEFINES += QN_EXPORT=Q_DECL_IMPORT }
  !isEmpty(BUILDLIB) { DEFINES += QN_EXPORT= }
}

DEFINES += __STDC_CONSTANT_MACROS

unix {
  LIBS += -lz -lcrypto
  QMAKE_CXXFLAGS += -msse2
  DEFINES += QN_EXPORT=
}

mac {
  LIBS += -lbz2 -framework IOKit -framework CoreServices
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
