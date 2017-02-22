TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/common_libs/nx_media/src

unix: !mac {
    # Ignore missing platform-dependent libs required for libproxydecoder.so
    LIBS += "-Wl,--allow-shlib-undefined"
}


