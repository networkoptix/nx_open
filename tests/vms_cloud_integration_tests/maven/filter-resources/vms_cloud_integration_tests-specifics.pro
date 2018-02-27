TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include
INCLUDEPATH += ${root.dir}/nx_cloud/libcloud_db/src/
INCLUDEPATH += ${root.dir}/mediaserver_core/src/
INCLUDEPATH += ${root.dir}/appserver2/src/

linux {
    QMAKE_CXXFLAGS += -Werror
}
LIBS += $$FESTIVAL_LIB
