TEMPLATE = app
CONFIG += console

INCLUDEPATH -= $$ROOT_DIR/common/src

linux {
    QMAKE_CXXFLAGS += -Werror
}
