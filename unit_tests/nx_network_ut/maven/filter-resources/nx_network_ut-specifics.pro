TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/common_libs/udt/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
