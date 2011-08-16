HEADERS += $$PWD/qtvaudiodevice.h \
        $$PWD/qtvsound.h

SOURCES += $$PWD/qtvaudiodevice.cpp \
        $$PWD/qtvsound.cpp

win* {
    win32: OPENAL_LIBS_PATH = $$PWD/../../contrib/openal/bin/win32
    win64: OPENAL_LIBS_PATH = $$PWD/../../contrib/openal/bin/win64

    INCLUDEPATH += $$PWD/../../contrib/openal/include
    LIBS += -L$$OPENAL_LIBS_PATH -lOpenAL32

    QMAKE_POST_LINK += cp -r $$OPENAL_LIBS_PATH/*.dll $$DESTDIR
}

mac {
    LIBS += -framework OpenAL
}
