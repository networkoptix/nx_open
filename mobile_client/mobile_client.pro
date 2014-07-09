TEMPLATE = app

QT += qml quick concurrent sql

CONFIG(debug, debug|release) {
  CONFIGURATION=debug
} else {
  CONFIGURATION=release
}

CLIENT_DIR = $$PWD/../client
COMMON_DIR = $$PWD/../common
EC2_DIR = $$PWD/../appserver2
BUILDENV_DIR = $$PWD/../build_environment
BUILDENV_BASE_DIR = C:/develop/buildenv


DEFINES += \
    DISABLE_ALL_VENDORS \
    DISABLE_THIRD_PARTY \
    DISABLE_COLDSTORE \
    DISABLE_MDNS \
    DISABLE_ARCHIVE \
    DISABLE_DATA_PROVIDERS \
    DISABLE_SOFTWARE_MOTION_DETECTION \
    DISABLE_SENDMAIL \
    DISABLE_SSL \

DEFINES += \
    QN_EXPORT= \

include($$COMMON_DIR/x64/optional_functionality.pri)


INCLUDEPATH += \
    $$PWD/src \
    $$COMMON_DIR/src \
    $$EC2_DIR/src \
    $$CLIENT_DIR/src \
    $$BUILDENV_BASE_DIR/include \

LIBS += \
    -L$$BUILDENV_DIR/target/lib/$$CONFIGURATION \
    -lcommon \
    -lappserver2 \

SOURCES += \
    src/main.cpp \
    src/context/context_aware.cpp \
    src/components/ec_connection.cpp \
    src/context/context.cpp

HEADERS += \
    src/context/context.h \
    src/context/context_aware.h \
    src/components/ec_connection.h

RESOURCES += \
    qml.qrc \

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)
