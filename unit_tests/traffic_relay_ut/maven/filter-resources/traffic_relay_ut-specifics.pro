CONFIG += console

DEFINES+=BOOST_BIND_NO_PLACEHOLDERS

INCLUDEPATH += ${root.dir}/nx_cloud/libtraffic_relay/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
