INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/

INCLUDEPATH -= $$ROOT_DIR/common/src

linux {
    QMAKE_CXXFLAGS += -Werror
    QMAKE_LFLAGS += -Wl,--no-undefined
}
