contains(QT_CONFIG, opengl): QT *= opengl

HEADERS += mainwindow.h \
           graphicsview.h

SOURCES += client_main.cpp \
           mainwindow.cpp \
           graphicsview.cpp

RESOURCES += images.qrc

include( $$PWD/widgets/widgets.pri )

include( $$PWD/videoitem.pri )

INCLUDEPATH += $$PWD
DEPENDPATH *= $$INCLUDEPATH
