HEADERS += $$PWD/ffmpegaudiodecoder_p.h \
        $$PWD/ffmpegvideodecoder_p.h

SOURCES += $$PWD/ffmpegaudiodecoder.cpp \
        $$PWD/ffmpegvideodecoder.cpp

win32-msvc*:0 { # disable for now...
    DEFINES += _USE_DXVA

    LIBS_PRIVATE += -ld3d9 -ldxva2

    HEADERS += $$PWD/dxva/dxva_p.h \
            $$PWD/dxva/dxva_copy_p.h \
            $$PWD/dxva/dxva_picture_p.h \
            $$PWD/dxva/ffmpeg_dxva_callbacks_p.h \

    SOURCES += $$PWD/dxva/dxva.cpp \
            $$PWD/dxva/dxva_copy.cpp \
            $$PWD/dxva/ffmpeg_dxva_callbacks.cpp
}
