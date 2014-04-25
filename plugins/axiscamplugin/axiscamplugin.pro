CONFIG += debug_and_release
TEMPLATE = lib
TARGET = axiscamplugin
QT += core network

INCLUDEPATH += ../../include/

CONFIG(debug, debug|release) {
  DESTDIR = debug
} else {
  DESTDIR = release
}
OBJECTS_DIR = $$DESTDIR
MOC_DIR = $$DESTDIR

win32 {
  QMAKE_CXXFLAGS += /wd4250
} else {
  DEFINES += override=
}

# Input
HEADERS += src/axis_cam_params.h \
           src/axis_camera_manager.h \
           src/axis_camera_plugin.h \
           src/axis_discovery_manager.h \
           src/axis_media_encoder.h \
           src/axis_relayio_manager.h \
           src/sync_http_client.h \
           src/sync_http_client_delegate.h

SOURCES += src/axis_camera_manager.cpp \
           src/axis_camera_plugin.cpp \
           src/axis_discovery_manager.cpp \
           src/axis_media_encoder.cpp \
           src/axis_relayio_manager.cpp \
           src/sync_http_client.cpp \
           src/sync_http_client_delegate.cpp
