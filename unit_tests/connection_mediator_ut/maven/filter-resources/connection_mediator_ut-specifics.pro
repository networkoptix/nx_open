TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include
INCLUDEPATH += ${root.dir}/nx_cloud/libconnection_mediator/src
INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src
INCLUDEPATH += ${root.dir}/vms_libs/nx_vms_api/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
