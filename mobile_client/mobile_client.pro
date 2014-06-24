TEMPLATE = app

QT += qml quick

CONFIG(debug, debug|release) {
  CONFIGURATION=debug
} else {
  CONFIGURATION=release
}

COMMON_DIR = $$PWD/../common
EC2_DIR = $$PWD/../appserver2
BUILDENV_DIR = $$PWD/../build_environment

INCLUDEPATH += \
    $$COMMON_DIR/src \
    $$EC2_DIR/src \

LIBS += \
    -L$$BUILDENV_DIR/target/lib/$$CONFIGURATION \
    -lcommon \
    -lappserver2 \

SOURCES += \
    src/main.cpp \

HEADERS += \

RESOURCES += \
    qml.qrc \

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)
