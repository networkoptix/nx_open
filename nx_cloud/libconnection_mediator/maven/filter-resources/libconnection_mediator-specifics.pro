TEMPLATE = lib
TARGET = connection_mediator
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include

SOURCES += ${project.build.directory}/libconnection_mediator_app_info_impl.cpp
