TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/common_libs/udt/src
INCLUDEPATH -= $$ROOT_DIR/common/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
