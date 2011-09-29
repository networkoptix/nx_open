TEMPLATE  = app
TARGET    = client
DESTDIR   = ../bin
CONFIG -= flat
CONFIG    *= qt warn_on debug_and_release

#ICON = eve_logo.icns
#QMAKE_INFO_PLIST = Info.plist

include( $$PWD/version.pri )

exists( $$OUT_PWD/build_version.pri ) {
    include( $$OUT_PWD/build_version.pri )
} else {
    exists( $$PWD/build_version.pri ) {
        include( $$PWD/build_version.pri )
    }
}

debug {
    DESTDIR = ../bin/debug
    OBJECTS_DIR = build/debug
    MOC_DIR = build/debug/generated
    UI_DIR = build/debug/generated
    RCC_DIR = build/debug/generated
    CONFIG += console
}

release {
    DESTDIR = ../bin/release
    OBJECTS_DIR = build/release
    MOC_DIR = build/release/generated
    UI_DIR = build/release/generated
    RCC_DIR = build/release/generated
}

PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

include( $$PWD/src/src.pri )
