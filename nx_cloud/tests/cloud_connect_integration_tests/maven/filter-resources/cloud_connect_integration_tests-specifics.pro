TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud
INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include
INCLUDEPATH += ${root.dir}/nx_cloud/libtraffic_relay/src
INCLUDEPATH += ${root.dir}/nx_cloud/libvms_gateway/src
INCLUDEPATH += ${root.dir}/nx_cloud/libconnection_mediator/src

INCLUDEPATH -= $$ROOT_DIR/common/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
