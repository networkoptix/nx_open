TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/common_libs/nx_update/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
