TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/nx_cloud/libconnection_mediator/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
