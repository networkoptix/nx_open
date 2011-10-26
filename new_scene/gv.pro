contains(QT_CONFIG, opengl): QT += opengl

CONFIG += console

HEADERS += mainwindow.h \

SOURCES += main.cpp \
           mainwindow.cpp \

RESOURCES += images.qrc

INCLUDEPATH += $$PWD

include( $$PWD/widgets/widgets.pri )
include( $$PWD/instruments/instruments.pri )
include( $$PWD/utility/utility.pri )
include( $$PWD/scene/scene.pri )
