QT += network xml
QT -= gui

TARGET = consoleapp
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += SessionManager.cpp Objects.cpp Main.cpp
HEADERS += SessionManager.h Objects.h Main.h
