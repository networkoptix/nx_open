contains(QT_CONFIG, opengl): QT += opengl

HEADERS += mainwindow.h \
           graphicsview.h \
           animatedwidget.h \
           layoutitem.h \
           celllayout.h \
           celllayout_p.h \

SOURCES += main.cpp \
           mainwindow.cpp \
           graphicsview.cpp \
           animatedwidget.cpp \
           layoutitem.cpp \
           celllayout.cpp \

RESOURCES += images.qrc
