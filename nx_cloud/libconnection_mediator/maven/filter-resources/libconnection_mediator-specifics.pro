TEMPLATE = lib
TARGET = connection_mediator
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include

include($$ADDITIONAL_QT_INCLUDES/qtsingleapplication/src/qtsinglecoreapplication.pri)
include($$ADDITIONAL_QT_INCLUDES/qtservice/src/qtservice.pri)

SOURCES += ${project.build.directory}/libconnection_mediator_app_info_impl.cpp
