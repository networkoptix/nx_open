CONFIG += console

win* {
    DEFINES += NX_VMS_UTILS_API=__declspec(dllexport)
}
