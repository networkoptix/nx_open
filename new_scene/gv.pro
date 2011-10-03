contains(QT_CONFIG, opengl): QT += opengl

HEADERS += mainwindow.h \
           graphicsview.h \
           animatedwidget.h \
#           interactiveitem.h \
#           chip.h \
           layoutitem.h \
           celllayout.h \
           celllayout_p.h \

SOURCES += main.cpp \
           mainwindow.cpp \
           graphicsview.cpp \
           animatedwidget.cpp \
#           interactiveitem.cpp \
#           chip.cpp \
           layoutitem.cpp \
           celllayout.cpp \

RESOURCES += images.qrc
