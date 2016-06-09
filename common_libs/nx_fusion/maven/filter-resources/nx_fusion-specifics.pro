CONFIG += console

win* {
    DEFINES += NX_FUSION_API=__declspec(dllexport)
}
