QT *= network xml

INCLUDEPATH += $$PWD/common $$PWD/common/core $$PWD/common/utils $$PWD/common/plugins

CONFIG(debug, debug|release) {
  INCLUDEPATH += $$FFMPEG-debug/include
  LIBS += -L$$FFMPEG-debug/bin -L$$FFMPEG-debug/lib
}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
  LIBS += -L$$FFMPEG-release/bin -L$$FFMPEG-release/lib
}

win32 {
  LIBS += -lws2_32 -lIphlpapi -lOle32
  LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR
    DEFINES += _CRT_SECURE_NO_WARNINGS
    INCLUDEPATH += $$PWD/../contrib/ffmpeg-misc-headers-win32
  }
}

mac {
  LIBS += -framework SystemConfiguration
  QMAKE_LFLAGS += -lavcodec.53 -lavdevice.53 -lavfilter.2 -lavformat.53 -lavutil.51 -lswscale.0 -lz -lbz2
}

DEFINES += __STDC_CONSTANT_MACROS
