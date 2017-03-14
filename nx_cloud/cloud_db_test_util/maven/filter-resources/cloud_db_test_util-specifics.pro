TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/appserver2/src
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
