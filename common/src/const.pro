TEMPLATE = lib
QT *= multimedia network xml
CONFIG += precompile_header %BUILDLIB
CONFIG -= flat

win32 {
  CONFIG += x86
}

FFMPEG = %FFMPEG
EVETOOLS_DIR = %EVETOOLS_DIR

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
  LIBS += -framework IOKit
  LIBS += -L../contrib/qjson/lib/mac
}

unix:!mac {
  LIBS += -L../contrib/qjson/lib/linux
}

DEFINES += __STDC_CONSTANT_MACROS


# Clone ssh://hg@vigasin.com/evetools to the same diectory netoptix_vms is located
win32 {
    QMAKE_CXXFLAGS += -Zc:wchar_t
    QMAKE_CXXFLAGS -= -Zc:wchar_t-
    LIBS += -lxerces-c_3
}

mac {
    LIBS += -lxerces-c-3.1
}

uniq:!mac {
    LIBS += -lxerces-c
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
            $$PWD/api/xsd/storages.xsd \
            $$PWD/api/xsd/scheduleTasks.xsd \
            $$PWD/api/xsd/events.xsd \
            $$PWD/api/xsd/videoserver/recordedTimePeriods.xsd

xsd2.name = Generating code from ${QMAKE_FILE_IN}
xsd2.input = XSD_FILES
xsd2.output = $${MOC_DIR}/xsd_${QMAKE_FILE_BASE}.cpp
xsd2.commands = $$EVETOOLS_DIR/bin/xsd cxx-tree --generate-serialization --output-dir $${MOC_DIR} --cxx-regex \"/^(.*).xsd/xsd_\\1.cpp/\" --hxx-regex \"/^(.*).xsd/xsd_\\1.h/\" --generate-ostream --root-element ${QMAKE_FILE_BASE} ${QMAKE_FILE_IN}
xsd2.CONFIG += target_predeps
xsd2.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += xsd2

PB_FILES = $$PWD/api/pb/camera.proto \
           $$PWD/api/pb/user.proto \
           $$PWD/api/pb/resourceType.proto \
           $$PWD/api/pb/resource.proto \
           $$PWD/api/pb/server.proto \
           $$PWD/api/pb/ms_recordedTimePeriod.proto

pb.name = Generating code from ${QMAKE_FILE_IN}
pb.input = PB_FILES
pb.output = $${MOC_DIR}/${QMAKE_FILE_BASE}.pb.cc
pb.commands = $$EVETOOLS_DIR/bin/protoc --proto_path=../src/api/pb --cpp_out=$${MOC_DIR} ${QMAKE_FILE_NAME}
pb.CONFIG += target_predeps
pb.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += pb

#RESOURCES += api/xsd/api.qrc
RESOURCES += $${MOC_DIR}/api.qrc

# Define override specifier.
OVERRIDE_DEFINITION = "override="
win32-msvc*:OVERRIDE_DEFINITION = "override=override"
DEFINES += $$OVERRIDE_DEFINITION
