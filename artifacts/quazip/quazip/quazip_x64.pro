TEMPLATE = lib
TARGET = quazip
INCLUDEPATH += .
QT -= gui

INCLUDEPATH += ..\..\..\buildenv\packages\windows-x86\qt-5.6.0\include\QtZlib

# This one handles dllimport/dllexport directives.
DEFINES += QUAZIP_BUILD

CONFIG += x64

QMAKE_LFLAGS += /MACHINE:x64 /LARGEADDRESSAWARE

VCPROJ_ARCH = x64

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
