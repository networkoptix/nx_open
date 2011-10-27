INCLUDEPATH += ../../eveCommon/src

win* {
  INCLUDEPATH += ../../eveCommon/contrib/ffmpeg-misc-headers-win32
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

TARGET = Server

win32: RC_FILE = server.rc

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

  # Define QN_EXPORT only if eveCommon build is not static
  isEmpty(BUILDLIB) { DEFINES += QN_EXPORT=Q_DECL_IMPORT }
  !isEmpty(BUILDLIB) { DEFINES += QN_EXPORT= }

}

mac {
  LIBS += -framework IOKit -framework CoreServices
  LIBS += -lz -lbz2

  QMAKE_CXXFLAGS += -msse4.1 -DAPI_TEST_MAIN
  PRIVATE_FRAMEWORKS.files = ../resource/arecontvision
  PRIVATE_FRAMEWORKS.path = Contents/MacOS
  QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS

  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f $$PWD/../resource/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision

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
