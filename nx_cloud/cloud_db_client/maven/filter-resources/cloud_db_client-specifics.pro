INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/

INCLUDEPATH -= $$ROOT_DIR/common/src \
               $$ROOT_DIR/common_libs/nx_streaming/src

linux {
    QMAKE_CXXFLAGS += -Werror
}

