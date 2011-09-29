HEADERS += $$PWD/abstractaudiodecoder.h \
        $$PWD/abstractvideodecoder.h \
        $$PWD/audiodata.h \
        $$PWD/videodata.h

SOURCES += $$PWD/abstractaudiodecoder.cpp \
        $$PWD/abstractvideodecoder.cpp \
        $$PWD/videodata.cpp

include( $$PWD/ffmpeg/ffmpeg.pri )
