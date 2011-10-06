contains(QT_CONFIG, opengl): QT += opengl

CONFIG += console

HEADERS += mainwindow.h \
           graphicsview.h

SOURCES += main.cpp \
           mainwindow.cpp \
           graphicsview.cpp

RESOURCES += images.qrc

include( $$PWD/widgets/widgets.pri )
