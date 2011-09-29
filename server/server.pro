QT *= core network xml xmlpatterns multimedia webkit
CONFIG += x86
CONFIG += qt console
CONFIG -= flat
TEMPLATE = app
VERSION = 0.0.1
ICON = eve_logo.icns
QMAKE_INFO_PLIST = Info.plist

TARGET = server
DESTDIR = ../bin

debug {
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = build/debug
  MOC_DIR = build/debug/generated
  UI_DIR = build/debug/generated
  RCC_DIR = build/debug/generated
}

release {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = build/release
  MOC_DIR = build/release/generated
  UI_DIR = build/release/generated
  RCC_DIR = build/release/generated
}

include( $$PWD/../common/src/common.pri )

win32 {
  RC_FILE = server.rc
}

mac {
  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f ../bin/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision

  PRIVATE_FRAMEWORKS.files = resource/arecontvision
  PRIVATE_FRAMEWORKS.path = Contents/MacOS
  QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = StdAfx.h
PRECOMPILED_SOURCE = StdAfx.cpp

include( $$PWD/src/src.pri )
