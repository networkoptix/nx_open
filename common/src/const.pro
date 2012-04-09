TEMPLATE = lib
QT *= network xml
CONFIG += precompile_header %BUILDLIB
CONFIG -= flat

win32 {
  CONFIG += x86
  QT *= multimedia
  INCLUDEPATH += $$PWD/../../common/contrib/openssl/include
  LIBS += -L$$PWD/../../common/contrib/openssl/bin -llibeay32
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
  win32 {
    LIBS+=-L../contrib/qjson/lib/win32/debug
  }
}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
  LIBS = -L$$FFMPEG-release/bin -L$$FFMPEG-release/lib $$LIBS
  win32 {
    LIBS+=-L../contrib/qjson/lib/win32/release
  }
}

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale -lqjson

INCLUDEPATH += ../contrib/qjson/include
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
  LIBS += -lz -lbz2
  DEFINES += QN_EXPORT=
  QMAKE_CXXFLAGS += -msse4.1
}

mac {
  LIBS += -framework IOKit -lcrypto
  LIBS += -L../contrib/qjson/lib/mac
}

unix:!mac {
  LIBS += -L../contrib/qjson/lib/linux
}

DEFINES += __STDC_CONSTANT_MACROS


unix {
    LIBS += -lprotobuf
}

QMAKE_CXXFLAGS += -I$$EVETOOLS_DIR/include 
LIBS += -L$$EVETOOLS_DIR/lib

# Define override specifier.
OVERRIDE_DEFINITION = "override="
win32-msvc*:OVERRIDE_DEFINITION = "override=override"
DEFINES += $$OVERRIDE_DEFINITION

PB_FILES = $$PWD/api/pb/camera.proto \
           $$PWD/api/pb/layout.proto \
           $$PWD/api/pb/license.proto \
           $$PWD/api/pb/user.proto \
           $$PWD/api/pb/resourceType.proto \
           $$PWD/api/pb/resource.proto \
           $$PWD/api/pb/server.proto \
           $$PWD/api/pb/ms_recordedTimePeriod.proto

pb.name = Generating code from ${QMAKE_FILE_IN}
pb.input = PB_FILES
pb.output = $${MOC_DIR}/${QMAKE_FILE_BASE}.pb.cc
pb.commands = $$EVETOOLS_DIR/bin/protoc --proto_path=../src/api/pb --cpp_out=$${MOC_DIR} ../src/api/pb/${QMAKE_FILE_BASE}.proto
pb.CONFIG += target_predeps
pb.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += pb
