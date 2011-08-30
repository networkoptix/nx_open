INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS = animator/basic.h animator/aprogress.h animator/hover.h \
          animator/hoverindex.h animator/hovercomplex.h animator/tab.h \
          blib/colors.h blib/gradients.h blib/tileset.h blib/FX.h blib/shapes.h blib/elements.h \
          blib/dpi.h blib/shadows.h \
          bespin.h visualframe.h draw.h config.h types.h debug.h hacks.h

SOURCES = animator/basic.cpp animator/aprogress.cpp animator/hover.cpp \
          animator/hoverindex.cpp animator/hovercomplex.cpp animator/tab.cpp \
          blib/colors.cpp blib/elements.cpp blib/dpi.cpp blib/tileset.cpp blib/gradients.cpp\
          blib/FX.cpp blib/shapes.cpp blib/shadows.cpp \
          bespin.cpp stylehint.cpp sizefromcontents.cpp qsubcmetrics.cpp \
          pixelmetric.cpp stdpix.cpp visualframe.cpp init.cpp genpixmaps.cpp polish.cpp \
          buttons.cpp docks.cpp frames.cpp input.cpp menus.cpp progress.cpp \
          scrollareas.cpp indicators.cpp slider.cpp tabbing.cpp toolbars.cpp \
          views.cpp window.cpp hacks.cpp

unix:x11 {
# this can talk to kwin
   HEADERS += blib/xproperty.h
   SOURCES += blib/xproperty.cpp

#    xrender is assumed on unix systems (linux) - if you don't want (stupid idea) or have (e.g. qtopia)
#    comment the line below with a '#' (compiling will fail otherwise)
   LIBS += -lXrender

# for macmenu and amarok hacks
   QT *= dbus
}

win32-msvc* {
    DEFINES += lround="(int)"
#    DEFINES += lround(num)="((num) < 0.0 ? ceil((num) - 0.5) : floor((num) + 0.5))"
    DEFINES += _CRT_SECURE_NO_WARNINGS
}
