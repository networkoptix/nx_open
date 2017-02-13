TEMPLATE = app
CONFIG += console

INCLUDEPATH += ${root.dir}/common_libs/nx_media/src

unix: !mac {
    #LIBS += "-Wl,-rpath-link,${libdir}/lib/$$CONFIGURATION/"
    #LIBS += "-Wl,-rpath-link,$$OPENSSL_DIR/lib"
    # Ignore missing platform-dependent libs required for libproxydecoder.so
    LIBS += "-Wl,--allow-shlib-undefined"
}


