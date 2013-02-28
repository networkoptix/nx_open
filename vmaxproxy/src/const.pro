QT = core
CONFIG += precompile_header
CONFIG -= flat app_bundle

win32 {
  CONFIG += x86
  LIBS += -lws2_32
}

TEMPLATE = app
VERSION = 1.0.0
ICON = mediaproxy.icns

TARGET = vmaxproxy


BUILDLIB = %BUILDLIB

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

CONFIG(debug, debug|release) {
  CONFIG += console
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = ../build/debug
  MOC_DIR = ../build/debug/generated
  UI_DIR = ../build/debug/generated
  RCC_DIR = ../build/debug/generated
}

CONFIG(release, debug|release) {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = ../build/release
  MOC_DIR = ../build/release/generated
  UI_DIR = ../build/release/generated
  RCC_DIR = ../build/release/generated
}

win32: RC_FILE = vmaxproxy.rc


DEFINES += __STDC_CONSTANT_MACROS

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

# Define override specifier.
OVERRIDE_DEFINITION = "override="
win32-msvc*:OVERRIDE_DEFINITION = "override=override"
DEFINES += $$OVERRIDE_DEFINITION

