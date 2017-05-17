TEMPLATE = app
CONFIG += console

INCLUDEPATH -= $$ROOT_DIR/common/src \
               $$ROOT_DIR/common_libs/nx_streaming/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
