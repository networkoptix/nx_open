TEMPLATE = lib
TARGET = cloud_db
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/nx_cloud/
INCLUDEPATH += ${root.dir}/common_libs/nx_email/src/

win* {
    DEFINES+=_VARIADIC_MAX=8
}

SOURCES += ${project.build.directory}/libcloud_db_app_info_impl.cpp
