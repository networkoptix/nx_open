HEADERS += $$PWD/qtvaudiodevice.h \
        $$PWD/qtvsound.h

SOURCES += $$PWD/qtvaudiodevice.cpp \
        $$PWD/qtvsound.cpp

win* {
    win32: OPENAL_LIBS_PATH = $$PWD/../../contrib/openal/bin/win32
    win64: OPENAL_LIBS_PATH = $$PWD/../../contrib/openal/bin/win64

    INCLUDEPATH += $$PWD/../../contrib/openal/include
    LIBS += -L$$OPENAL_LIBS_PATH -lOpenAL32

    OPENAL_LIBS_DESTDIR = $$DESTDIR
    win32-msvc* {
        OPENAL_LIBS_PATH ~= s,/,\\,
        OPENAL_LIBS_DESTDIR ~= s,/,\\,
    }
    QMAKE_POST_LINK += $$QMAKE_COPY $$OPENAL_LIBS_PATH$${DIR_SEPARATOR}*.dll $$OPENAL_LIBS_DESTDIR
}

mac {
    LIBS += -framework OpenAL
}
