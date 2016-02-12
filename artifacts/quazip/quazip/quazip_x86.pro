TEMPLATE = lib
TARGET = quazip
INCLUDEPATH += .
QT -= gui

# This one handles dllimport/dllexport directives.
DEFINES += QUAZIP_BUILD

LIBS += -Lc:/develop/buildenv/artifacts/qt/5.6.0/windows/x86/lib

INCLUDEPATH += c:/develop/buildenv/artifacts/qt/5.6.0/windows/x86/include 
INCLUDEPATH += c:/develop/buildenv/artifacts/qt/5.6.0/windows/x86/include/QtCore 
INCLUDEPATH += c:/develop/buildenv/artifacts/qt/5.6.0/windows/x86/include/QtZlib

# Input
HEADERS += crypt.h \
           ioapi.h \
           JlCompress.h \
           quaadler32.h \
           quachecksum32.h \
           quacrc32.h \
           quagzipfile.h \
           quaziodevice.h \
           quazip.h \
           quazip_global.h \
           quazipdir.h \
           quazipfile.h \
           quazipfileinfo.h \
           quazipnewinfo.h \
           unzip.h \
           zip.h
SOURCES += JlCompress.cpp \
           qioapi.cpp \
           quaadler32.cpp \
           quacrc32.cpp \
           quagzipfile.cpp \
           quaziodevice.cpp \
           quazip.cpp \
           quazipdir.cpp \
           quazipfile.cpp \
           quazipfileinfo.cpp \
           quazipnewinfo.cpp \
           unzip.c \
           zip.c
