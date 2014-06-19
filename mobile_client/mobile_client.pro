TEMPLATE = app

QT += qml quick

COMMON_DIR = ../common

SOURCES += \
    src/main.cpp \
    $$COMMON_DIR/

HEADERS += \


RESOURCES += \
    qml.qrc \

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)
