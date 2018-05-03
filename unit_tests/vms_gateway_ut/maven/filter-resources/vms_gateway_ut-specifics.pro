TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/common_libs/nx_fusion/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include
INCLUDEPATH += ${root.dir}/nx_cloud/libvms_gateway_core/src
INCLUDEPATH += ${root.dir}/nx_cloud/
INCLUDEPATH += ${root.dir}/nx_cloud/librelaying/src/

INCLUDEPATH -= $$ROOT_DIR/common/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
