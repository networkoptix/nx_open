QT = core gui network xml xmlpatterns opengl multimedia webkit 
CONFIG += x86
TEMPLATE = app
VERSION = 0.0.1

TARGET = eveplayer_beta

debug {
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = ../build/debug
  MOC_DIR = ../build/debug/generated
  UI_DIR = ../build/debug/generated
  RCC_DIR = ../build/debug/generated
}

release {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = ../build/release
  MOC_DIR = ../build/release/generated
  UI_DIR = ../build/release/generated
  RCC_DIR = ../build/release/generated
}


win32 {
  QMAKE_CXXFLAGS += -MP /Fd$(IntDir)
  INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
  INCLUDEPATH += ../contrib/$$FFMPEG/include
  RC_FILE = uniclient.rc
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

RESOURCES += mainwnd.qrc ../build/skin.qrc
FORMS += mainwnd.ui preferences.ui licensekey.ui

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

