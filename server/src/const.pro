QT = core network xml xmlpatterns multimedia webkit 
CONFIG += x86
CONFIG += qt console
TEMPLATE = app
VERSION = 0.0.1
ICON = eve_logo.icns
QMAKE_INFO_PLIST = Info.plist

INCLUDEPATH += ./common/core
INCLUDEPATH += ./common/utils

TARGET = server

debug {
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = ../build/debug
  MOC_DIR = ../build/debug/generated
  UI_DIR = ../build/debug/generated
  RCC_DIR = ../build/debug/generated
  INCLUDEPATH += $$FFMPEG-debug/include
}

release {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = ../build/release
  MOC_DIR = ../build/release/generated
  UI_DIR = ../build/release/generated
  RCC_DIR = ../build/release/generated
  INCLUDEPATH += $$FFMPEG-release/include
}


win32 {
  QMAKE_CXXFLAGS += -MP /Fd$(IntDir)
  INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
  INCLUDEPATH += ../contrib/openal/include
  RC_FILE = server.rc
}

mac {
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
  QMAKE_LFLAGS_DEBUG += /libpath:$$FFMPEG-debug/bin
  QMAKE_LFLAGS_RELEASE += /libpath:$$FFMPEG-release/bin
  
  LIBS += ../contrib/openal/bin/win32/OpenAL32.lib
}  

mac {
  LIBS += -framework SystemConfiguration
  LIBS += -framework OpenAL
  QMAKE_LFLAGS += -lavcodec.53 -lavdevice.53 -lavfilter.2 -lavformat.53 -lavutil.51 -lswscale.0 -lz -lbz2
  QMAKE_LFLAGS_DEBUG += -L$$FFMPEG-debug/lib
  QMAKE_LFLAGS_RELEASE += -L$$FFMPEG-release/lib
  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f ../bin/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision
}

