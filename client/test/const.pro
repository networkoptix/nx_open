QT = core gui network xml xmlpatterns opengl multimedia webkit
CONFIG += x86 console
TEMPLATE = app
VERSION = 0.0.1

TARGET = uniclient-tests

debug {
  DESTDIR = ../bin/debug-test
  OBJECTS_DIR  = ../build/debug-test
  MOC_DIR = ../build/debug-test/generated
  UI_DIR = ../build/debug-test/generated
  RCC_DIR = ../build/debug-test/generated
  INCLUDEPATH += $$FFMPEG-debug/include
}

release {
  DESTDIR = ../bin/release-test
  OBJECTS_DIR  = ../build/release-test
  MOC_DIR = ../build/release-test/generated
  UI_DIR = ../build/release-test/generated
  RCC_DIR = ../build/release-test/generated
  INCLUDEPATH += $$FFMPEG-release/include
}


win32 {
  QMAKE_CXXFLAGS += -MP /Fd$(IntDir) -DUNICLIENT_TESTS
  INCLUDEPATH += ../src
  INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
  RC_FILE = ../src/uniclient.rc
}

mac {
  INCLUDEPATH += /Users/ivan/opt/ffmpeg/include

  PRIVATE_FRAMEWORKS.files = ../resource/arecontvision
  PRIVATE_FRAMEWORKS.path = Contents/MacOS
  QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = StdAfx.h
PRECOMPILED_SOURCE = StdAfx.cpp

DEFINES += __STDC_CONSTANT_MACROS

win32 {
  LIBS += ws2_32.lib Iphlpapi.lib
  QMAKE_LFLAGS += avcodec-53.lib avdevice-53.lib avfilter-2.lib avformat-53.lib avutil-51.lib swscale-0.lib
  QMAKE_LFLAGS_DEBUG += /libpath:../contrib/$$FFMPEG/bin/debug
  QMAKE_LFLAGS_RELEASE += /libpath:../contrib/$$FFMPEG/bin/release
}

mac {
  LIBS += -framework SystemConfiguration
  QMAKE_LFLAGS += -lavcodec.53 -lavdevice.53 -lavfilter.2 -lavformat.53 -lavutil.51 -lswscale.0 -lz -lbz2 -L/Users/ivan/opt/ffmpeg/lib
  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f ../bin/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision
}
