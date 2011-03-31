QT = core gui network xml xmlpatterns opengl multimedia webkit 
CONFIG += x86
TEMPLATE = app
VERSION = 0.0.1

TARGET = uniclient

debug {
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = ../build/debug
  MOC_DIR = ../build/debug/generated
  UI_DIR = ../build/debug/generated
}

release {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = ../build/release
  MOC_DIR = ../build/release/generated
  UI_DIR = ../build/release/generated
}


win32 {
  QMAKE_CXXFLAGS += -MP /Fd$(IntDir)
  INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
  INCLUDEPATH += ../contrib/$$FFMPEG/include
  RC_FILE = uniclient.rc
}

mac {
  INCLUDEPATH += /Users/ivan/opt/ffmpeg/include
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = StdAfx.h
PRECOMPILED_SOURCE = StdAfx.cpp

DEFINES += __STDC_CONSTANT_MACROS

RESOURCES += mainwnd.qrc skin.qrc
FORMS += mainwnd.ui preferences.ui

win32 {
  QMAKE_LFLAGS += avcodec-52.lib avdevice-52.lib avfilter-1.lib avformat-52.lib avutil-50.lib swscale-0.lib
  QMAKE_LFLAGS_DEBUG += /libpath:../contrib/$$FFMPEG/bin/debug
  QMAKE_LFLAGS_RELEASE += /libpath:../contrib/$$FFMPEG/bin/release
}  

mac {
  LIBS += -framework SystemConfiguration
  QMAKE_LFLAGS += -lavcodec.52 -lavdevice.52 -lavfilter.1 -lavformat.52 -lavutil.50 -lswscale.0 -lz -lbz2 -L/Users/ivan/opt/ffmpeg/lib
  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f ../bin/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision
}

