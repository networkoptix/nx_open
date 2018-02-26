TEMPLATE = app
CONFIG += console

linux {
    QMAKE_CXXFLAGS += -Werror
}
