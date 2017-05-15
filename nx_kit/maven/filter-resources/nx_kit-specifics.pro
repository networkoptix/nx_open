TEMPLATE = lib
LIBTYPE = ${buildLib}
GUID = ${guid}

QT -= core gui

equals(LIBTYPE, "staticlib"): CONFIG += static
else: CONFIG += shared

CONFIG(debug, debug|release) {
  CONFIGURATION = debug
} else {
  CONFIGURATION = release
}

DESTDIR = $$clean_path("${libdir}")/bin/$$CONFIGURATION

SRCDIR = ${packages.dir}/any/nx_kit/src

HEADERS += \
    $$SRCDIR/nx/kit/debug.h \
    $$SRCDIR/nx/kit/ini_config.h \
    $$SRCDIR/nx/kit/test.h \

SOURCES += \
    $$SRCDIR/nx/kit/debug.cpp \
    $$SRCDIR/nx/kit/ini_config.cpp \
    $$SRCDIR/nx/kit/test.cpp \

win*: DEFINES += NX_KIT_API=__declspec(dllexport)
