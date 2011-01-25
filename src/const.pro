QT = core gui network xml opengl multimedia webkit
TEMPLATE = app
VERSION = 0.0.1

TARGET = uniclient
DESTDIR = ../bin

INCLUDEPATH += ../contrib/ffmpeg26404/include

RESOURCES += mainwnd.qrc
FORMS += mainwnd.ui
#LIBS += ../contrib/ffmpeg/libs

