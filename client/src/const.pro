INCLUDEPATH += ../../common/src
INCLUDEPATH += ../../common/contrib/qjson/include

win* {
  INCLUDEPATH += ../../common/contrib/ffmpeg-misc-headers-win32
}

QT = core gui network xml opengl multimedia webkit
CONFIG += x86 precompile_header
CONFIG -= flat
TEMPLATE = app
VERSION = 0.0.1
ICON = eve_logo.icns
QMAKE_INFO_PLIST = Info.plist

TARGET = client

BUILDLIB = %BUILDLIB
FFMPEG = %FFMPEG
EVETOOLS_DIR = %EVETOOLS_DIR

include(../contrib/qtsingleapplication/src/qtsingleapplication.pri)

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

QMAKE_CXXFLAGS += -I$$EVETOOLS_DIR/include

CONFIG(debug, debug|release) {
  CONFIG += console
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = ../build/debug
  MOC_DIR = ../build/debug/generated
  UI_DIR = ../build/debug/generated
  RCC_DIR = ../build/debug/generated
}

CONFIG(release, debug|release) {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = ../build/release
  MOC_DIR = ../build/release/generated
  UI_DIR = ../build/release/generated
  RCC_DIR = ../build/release/generated
}

win32: RC_FILE = uniclient.rc

win* {
    !contains(QMAKE_HOST.arch, x86_64) {
        OPENAL_LIBS_PATH = $$PWD/../contrib/openal/bin/win32
    } else {
        OPENAL_LIBS_PATH = $$PWD/../contrib/openal/bin/win64
    }
    INCLUDEPATH += $$PWD/../contrib/openal/include
    LIBS += -L$$OPENAL_LIBS_PATH -lOpenAL32

    INCLUDEPATH += $$PWD/../contrib/openssl/include
    LIBS += -L$$PWD/../contrib/openssl/bin -llibeay32
}

mac {
    LIBS += -framework OpenAL

    LIBS += -lcrypto
}

win32 {
    QMAKE_CXXFLAGS += -Zc:wchar_t
    QMAKE_CXXFLAGS -= -Zc:wchar_t-
    LIBS += -lxerces-c_3

    # Define QN_EXPORT only if common build is not static
    isEmpty(BUILDLIB) { DEFINES += QN_EXPORT=Q_DECL_IMPORT }
    !isEmpty(BUILDLIB) { DEFINES += QN_EXPORT= }
}

mac {
    LIBS += -L../../common/contrib/qjson/lib/mac -lxerces-c-3.1
    DEFINES += QN_EXPORT=
}

LIBS += -L$$EVETOOLS_DIR/lib

CONFIG(debug, debug|release) {
  INCLUDEPATH += $$FFMPEG-debug/include
  LIBS += -L$$FFMPEG-debug/bin -L$$FFMPEG-debug/lib -L$$PWD/../../common/bin/debug -lcommon -L../../common/contrib/qjson/lib/win32/debug
}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
  LIBS += -L$$FFMPEG-release/bin -L$$FFMPEG-release/lib -L$$PWD/../../common/bin/release -lcommon -L../../common/contrib/qjson/lib/win32/release
}

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale -lqjson

win32 {
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR

    # Don't warn for deprecated 'unsecure' CRT functions.
    DEFINES += _CRT_SECURE_NO_WARNINGS

    # Don't warn for deprecated POSIX functions.
    DEFINES += _CRT_NONSTDC_NO_DEPRECATE

    # Disable warning C4250: 'Derived' : inherits 'Base::method' via dominance.
    # It is buggy, as described in http://connect.microsoft.com/VisualStudio/feedback/details/101259/disable-warning-c4250-class1-inherits-class2-member-via-dominance-when-weak-member-is-a-pure-virtual-function
    QMAKE_CXXFLAGS += /wd4250

    INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
    INCLUDEPATH += "$$(DXSDK_DIR)/Include"
    !contains(QMAKE_HOST.arch, x86_64) {
      LIBS += -L"$$(DXSDK_DIR)/Lib/x86"
    } else {
      LIBS += -L"$$(DXSDK_DIR)/Lib/x64"
    }
  }

  LIBS += -lws2_32 -lIphlpapi -lOle32

  LIBS += -ld3d9 -ld3dx9 -lwinmm

  SOURCES += dsp_effects/speex/preprocess.c dsp_effects/speex/filterbank.c dsp_effects/speex/fftwrap.c dsp_effects/speex/smallft.c  dsp_effects/speex/mdf.c
}

mac {
  LIBS += -framework IOKit -framework CoreServices
  LIBS += -lz -lbz2

  QMAKE_CXXFLAGS += -msse4.1
  PRIVATE_FRAMEWORKS.files = ../resource/arecontvision
  PRIVATE_FRAMEWORKS.path = Contents/MacOS
  QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS

  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f $$PWD/../resource/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision

#  QMAKE_CXXFLAGS += -DAPI_TEST_MAIN
#  TARGET = consoleapp
#  CONFIG   += console
#  CONFIG   -= app_bundle
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

RESOURCES += ../build/skin.qrc
FORMS += ui/preferences/connectionssettingswidget.ui ui/preferences/licensewidget.ui ui/preferences/preferences.ui ui/preferences/recordingsettingswidget.ui \
         ui/dialogs/logindialog.ui ui/dialogs/tagseditdialog.ui \
         youtube/youtubeuploaddialog.ui youtube/youtubesetting.ui \
         ui/device_settings/camera_schedule.ui

DEFINES += CL_TRIAL_MODE CL_FORCE_LOGO
#DEFINES += CL_CUSTOMIZATION_PRESET=\\\"trinity\\\"
# or
#DEFINES += CL_SKIN_PATH=\\\"./trinity\\\"


# Define override specifier.
OVERRIDE_DEFINITION = "override="
win32-msvc*:OVERRIDE_DEFINITION = "override=override"
DEFINES += $$OVERRIDE_DEFINITION
