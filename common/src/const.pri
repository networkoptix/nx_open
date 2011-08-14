QT *= network xml

INCLUDEPATH += $$PWD/common $$PWD/common/core $$PWD/common/utils $$PWD/common/plugins

CONFIG(debug, debug|release) {
  INCLUDEPATH += $$FFMPEG-debug/include
}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
}

win32 {
  LIBS += ws2_32.lib Iphlpapi.lib Ole32.lib
  LIBS += avcodec.lib avdevice.lib avfilter.lib avformat.lib avutil.lib swscale.lib
  QMAKE_LFLAGS_DEBUG += /libpath:$$FFMPEG-debug/bin
  QMAKE_LFLAGS_RELEASE += /libpath:$$FFMPEG-release/bin
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR
    DEFINES += _CRT_SECURE_NO_WARNINGS
    INCLUDEPATH += $$PWD/../contrib/ffmpeg-misc-headers-win32
  }
}

mac {
  LIBS += -framework SystemConfiguration
  QMAKE_LFLAGS += -lavcodec.53 -lavdevice.53 -lavfilter.2 -lavformat.53 -lavutil.51 -lswscale.0 -lz -lbz2
  QMAKE_LFLAGS_DEBUG += -L$$FFMPEG-debug/lib
  QMAKE_LFLAGS_RELEASE += -L$$FFMPEG-release/lib
}

DEFINES += __STDC_CONSTANT_MACROS
