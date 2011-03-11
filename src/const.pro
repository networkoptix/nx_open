QT = core gui network xml xmlpatterns opengl multimedia webkit 
TEMPLATE = app
VERSION = 0.0.1

TARGET = uniclient
DESTDIR = ../bin

QMAKE_CXXFLAGS += -MP

PRECOMPILED_HEADER = StdAfx.h
PRECOMPILED_SOURCE = StdAfx.cpp

INCLUDEPATH += ../contrib/ffmpeg-git-aecd0a4/include
DEFINES += __STDC_CONSTANT_MACROS

RESOURCES += mainwnd.qrc
FORMS += mainwnd.ui

QMAKE_LFLAGS += avcodec-52.lib avdevice-52.lib avfilter-1.lib avformat-52.lib avutil-50.lib swscale-0.lib d3d9.lib dxva2.lib
QMAKE_LFLAGS_DEBUG += /libpath:../contrib/ffmpeg-git-aecd0a4/bin/debug
QMAKE_LFLAGS_RELEASE += /libpath:../contrib/ffmpeg-git-aecd0a4/bin/release
