QT = core gui network xml xmlpatterns opengl multimedia webkit 
CONFIG += x86
TEMPLATE = app
VERSION = 0.0.1

TARGET = uniclient
DESTDIR = ../bin

MOC_DIR = generated

win32 {
  QMAKE_CXXFLAGS += -MP
  INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
  INCLUDEPATH += ../contrib/ffmpeg-git-aecd0a4/include
  RC_FILE = uniclient.rc
}

mac {
  INCLUDEPATH += /Users/ivan/opt/ffmpeg/include
  INCLUDEPATH += $$PWD
}

PRECOMPILED_HEADER = StdAfx.h
PRECOMPILED_SOURCE = StdAfx.cpp

DEFINES += __STDC_CONSTANT_MACROS

RESOURCES += mainwnd.qrc skin.qrc
FORMS += mainwnd.ui

win32 {
  QMAKE_LFLAGS += avcodec-52.lib avdevice-52.lib avfilter-1.lib avformat-52.lib avutil-50.lib swscale-0.lib
  QMAKE_LFLAGS_DEBUG += /libpath:../contrib/ffmpeg-git-aecd0a4/bin/debug
  QMAKE_LFLAGS_RELEASE += /libpath:../contrib/ffmpeg-git-aecd0a4/bin/release
}  

mac {
  LIBS += -framework SystemConfiguration
  QMAKE_LFLAGS += -lavcodec.52 -lavdevice.52 -lavfilter.1 -lavformat.52 -lavutil.50 -lswscale.0 -lz -lbz2 -L/Users/ivan/opt/ffmpeg/lib
  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f ../bin/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision
}

