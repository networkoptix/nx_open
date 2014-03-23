TEMPLATE = lib
QT *= network xml
CONFIG += precompile_header %BUILDLIB
CONFIG -= flat


INCLUDEPATH += $$PWD/../../common/contrib/openssl/include
INCLUDEPATH += $$PWD/../../common/contrib/openssl/include/openssl/crypto


win32 {
  CONFIG += x86
  QT *= multimedia
  
  LIBS += -L$$PWD/../../common/contrib/openssl/bin -llibeay32 -ssleay32
  win32-msvc2010 {
    LIBS += -L$$PWD/../../common/contrib/openssl/bin/win32-msvc2010
  }
  win32-msvc2008 {
    LIBS += -L$$PWD/../../common/contrib/openssl/bin
  }
  LIBS += -llibeay32
}

FFMPEG = %FFMPEG
EVETOOLS_DIR = %EVETOOLS_DIR

INCLUDEPATH += $$PWD $$PWD/../build/generated
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

INCLUDEPATH += $$PWD/../../common/contrib/qt
SOURCES += ../contrib/qt/QtCore/private/qsimd.cpp

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
  LIBS = -L$$FFMPEG-debug/bin -L$$FFMPEG-debug/lib $$LIBS
}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
  LIBS = -L$$FFMPEG-release/bin -L$$FFMPEG-release/lib $$LIBS
}

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale

win32 {
  INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32 
  LIBS += -lws2_32 -lIphlpapi -lOle32 
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR

    # Don't warn for deprecated 'unsecure' CRT functions.
    DEFINES += _CRT_SECURE_NO_WARNINGS

    # Don't warn for deprecated POSIX functions.
    DEFINES += _CRT_NONSTDC_NO_DEPRECATE 

    # Disable warning C4250: 'Derived' : inherits 'Base::method' via dominance.
    # It is buggy, as described in http://connect.microsoft.com/VisualStudio/feedback/details/101259/disable-warning-c4250-class1-inherits-class2-member-via-dominance-when-weak-member-is-a-pure-virtual-function
    QMAKE_CXXFLAGS += /wd4250
  }

  !staticlib {
    DEFINES += QN_EXPORT=Q_DECL_EXPORT
  }

  staticlib {
    DEFINES += QN_EXPORT=
  }
}

unix {
  DEFINES += QN_EXPORT=
}

mac {
  QMAKE_CXXFLAGS += -msse4.1
  QMAKE_CFLAGS += -msse4.1
  LIBS += -framework CoreFoundation -framework IOKit -lcrypto
}

unix:!mac {
  QMAKE_CXXFLAGS += -msse2
  LIBS += -lz -lbz2
}

DEFINES += __STDC_CONSTANT_MACROS

INCLUDEPATH += $$EVETOOLS_DIR/include $$EVETOOLS_DIR/include/dvr
LIBS += -L$$EVETOOLS_DIR/lib

PROTOC_FILE = $$EVETOOLS_DIR/bin/protoc

unix {
    LIBS += -L/usr/lib
    QMAKE_CXXFLAGS += -I$$/usr/include
    PROTOC_FILE = /usr/bin/protoc
}

# Define override specifier.
OVERRIDE_DEFINITION = "override="
win32-msvc*:OVERRIDE_DEFINITION = "override=override"
DEFINES += $$OVERRIDE_DEFINITION


