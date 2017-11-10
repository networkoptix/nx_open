CONFIG += console

DEFINES+=BOOST_BIND_NO_PLACEHOLDERS

INCLUDEPATH += ${root.dir}/nx_cloud/librelaying/src

INCLUDEPATH -= $$ROOT_DIR/common/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
