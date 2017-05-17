win* {
    DEFINES += NX_FUSION_API=__declspec(dllexport)
}

INCLUDEPATH -= $$ROOT_DIR/common/src \
               $$ROOT_DIR/common_libs/nx_streaming/src
