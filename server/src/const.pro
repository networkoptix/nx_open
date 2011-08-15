QT *= core network xml xmlpatterns multimedia webkit
CONFIG += x86
CONFIG += qt console
CONFIG -= flat
TEMPLATE = app
VERSION = 0.0.1
ICON = eve_logo.icns
QMAKE_INFO_PLIST = Info.plist

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

include( $$PWD/../../common/src/common.pri )

win32 {
  INCLUDEPATH += ../contrib/openal/include
  LIBS += ../contrib/openal/bin/win32/OpenAL32.lib
  RC_FILE = server.rc
}

mac {
  LIBS += -framework OpenAL
  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f ../bin/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision

  PRIVATE_FRAMEWORKS.files = ../resource/arecontvision
  PRIVATE_FRAMEWORKS.path = Contents/MacOS
  QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = StdAfx.h
PRECOMPILED_SOURCE = StdAfx.cpp

