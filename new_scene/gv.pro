contains(QT_CONFIG, opengl): QT += opengl

HEADERS += mainwindow.h \
           graphicsview.h \
           animatedwidget.h \
#           interactiveitem.h \
#           chip.h \
           layoutitem.h

SOURCES += main.cpp \
           mainwindow.cpp \
           graphicsview.cpp \
           animatedwidget.cpp \
#           interactiveitem.cpp \
#           chip.cpp \
           layoutitem.cpp

RESOURCES += images.qrc
