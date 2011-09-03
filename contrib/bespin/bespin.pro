TEMPLATE = lib
CONFIG   += plugin
CONFIG   += debug_and_release build_all
DESTDIR  = $(QTDIR)/plugins/styles
TARGET = bespin

TARGET = $$qtLibraryTarget($$TARGET)

{
    *msvc*: PLATFORM = msvc
    win*-g++*: PLATFORM = mingw
    linux*: PLATFORM = linux
    mac*: PLATFORM = mac

    *64*: BITS = x64
    else: BITS = x86

    DESTDIR = $$PWD/bin/$$PLATFORM-$$BITS
    CONFIG(debug, debug|release): DESTDIR = $$DESTDIR/debug
    CONFIG(release, debug|release): DESTDIR = $$DESTDIR/release
    DESTDIR  = $$DESTDIR/styles
}

DEFINES += BLIB_EXPORT=""

include( $$PWD/src/bespin.pri )

SOURCES += $$PWD/bespinstyleplugin.cpp

DEFINES += BESPIN_REGISTRY_REPLACEMENT_PATH=\\\":/bespin/bespin.ini\\\"
RESOURCES += $$PWD/bespin.qrc

target.path += $$[QT_INSTALL_PLUGINS]/styles
INSTALLS += target
