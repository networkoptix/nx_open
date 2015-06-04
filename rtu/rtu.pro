TEMPLATE = app

QT += qml quick

QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH +=                      \
    src

SOURCES += \
    src/main.cpp \
    src/models/servers_selection_model.cpp \
    src/helpers/servers_finder.cpp \
    src/server_info.cpp \
    src/models/ip_settings_model.cpp \
    src/rtu_context.cpp \
    src/helpers/http_client.cpp \
    src/models/time_zones_model.cpp \
    src/requests/requests.cpp \
    src/helpers/logger_manager.cpp \
    src/change_request_manager.cpp \
    src/models/changes_summary_model.cpp \
    src/helpers/model_change_helper.cpp \
    src/models/server_changes_model.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

HEADERS += \
    src/models/servers_selection_model.h \
    src/server_info.h \
    src/helpers/servers_finder.h \
    src/models/ip_settings_model.h \
    src/models/types.h \
    src/rtu_context.h \
    src/helpers/http_client.h \
    src/models/time_zones_model.h \
    src/requests/requests.h \
    src/helpers/login_manager.h \
    src/change_request_manager.h \
    src/models/changes_summary_model.h \
    src/helpers/model_change_helper.h \
    src/models/server_changes_model.h
