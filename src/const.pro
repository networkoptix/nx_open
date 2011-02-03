QT = core gui network xml xmlpatterns opengl multimedia webkit 
TEMPLATE = app
VERSION = 0.0.1

TARGET = uniclient
DESTDIR = ../bin

QMAKE_CXXFLAGS += -MP

INCLUDEPATH += ../contrib/ffmpeg26404/include
DEFINES += __STDC_CONSTANT_MACROS

RESOURCES += mainwnd.qrc
FORMS += mainwnd.ui
#LIBS += ../contrib/ffmpeg/libs

