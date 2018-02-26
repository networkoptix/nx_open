CONFIG += console

win32 {
    DEFINES += NOMINMAX
}
INCLUDEPATH += ${root.dir}/mediaserver_core/src
INCLUDEPATH += ${root.dir}/appserver2/src
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/nx_cloud/

LIBS += $$FESTIVAL_LIB

