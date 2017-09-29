CONFIG += console

DEFINES+=BOOST_BIND_NO_PLACEHOLDERS

INCLUDEPATH += ${root.dir}/nx_cloud/libtraffic_relay/src
INCLUDEPATH += ${root.dir}/common_libs/nx_cassandra/src

INCLUDEPATH -= $$ROOT_DIR/common/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
