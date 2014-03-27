
INCLUDEPATH += ../../common/src
INCLUDEPATH += ../../common/contrib/qt

win* {
  INCLUDEPATH += ../../common/contrib/ffmpeg-misc-headers-win32
}

QT = core gui network xml opengl webkit multimedia
CONFIG += precompile_header
CONFIG -= flat app_bundle

win32 {
  CONFIG += x86
}

TEMPLATE = app
VERSION = 0.0.1
ICON = eve_logo.icns
QMAKE_INFO_PLIST = Info.plist

TARGET = client

BUILDLIB = %BUILDLIB
FFMPEG = %FFMPEG
EVETOOLS_DIR = %EVETOOLS_DIR

include(../../common/contrib/qtsingleapplication/src/qtsingleapplication.pri)

unix {
include(../contrib/qtcolorpicker/src/qtcolorpicker.pri)
}

TRANSLATIONS += # help/context_help_en.ts
RESOURCES += # help/context_help.qrc

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

QMAKE_CXXFLAGS += -I$$EVETOOLS_DIR/include

# Boris build doesn't use const.pro
QMAKE_CXXFLAGS += -DNO_TRANSLATIONS

unix {
  QMAKE_CXXFLAGS += -I/usr/include
}

CONFIG(debug, debug|release) {
  CONFIG += console
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = ../build/debug
  MOC_DIR = ../build/debug/generated
  UI_DIR = ../build/debug/generated
  RCC_DIR = ../build/debug/generated
  win32 {
  PRE_TARGETDEPS += ../../common/bin/debug/common.lib
  }
  unix:!mac {
  PRE_TARGETDEPS += ../../common/bin/debug/libcommon.a
  }
  mac {
  PRE_TARGETDEPS += ../../common/bin/debug/libcommon.dylib
  }
}

CONFIG(release, debug|release) {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = ../build/release
  MOC_DIR = ../build/release/generated
  UI_DIR = ../build/release/generated
  RCC_DIR = ../build/release/generated
  win32 {
  PRE_TARGETDEPS += ../../common/bin/release/common.lib
  }
  unix:!mac {
  PRE_TARGETDEPS += ../../common/bin/release/libcommon.a
  }
  mac {
  PRE_TARGETDEPS += ../../common/bin/release/libcommon.dylib
  }
}

win32: RC_FILE = uniclient.rc

win* {
    !contains(QMAKE_HOST.arch, x86_64) {
        OPENAL_LIBS_PATH = $$PWD/../contrib/openal/bin/win32
    } else {
        OPENAL_LIBS_PATH = $$PWD/../contrib/openal/bin/win64
    }
    LIBS += -L$$OPENAL_LIBS_PATH -lOpenAL32

    INCLUDEPATH += $$PWD/../../common/contrib/openssl/include
    INCLUDEPATH += $$PWD/../contrib/openal/include

    win32-msvc2010 {
        LIBS += -L$$PWD/../../common/contrib/openssl/bin/win32-msvc2010
    }
    win32-msvc2008 {
        LIBS += -L$$PWD/../../common/contrib/openssl/bin
    }
    LIBS += -llibeay32
}

mac {
    LIBS += -framework OpenAL
}

win32 {
    # Define QN_EXPORT only if common build is not static
    isEmpty(BUILDLIB) { DEFINES += QN_EXPORT=Q_DECL_IMPORT }
    !isEmpty(BUILDLIB) { DEFINES += QN_EXPORT= }
}

unix {
  LIBS += -lcrypto -lz
  QMAKE_CXXFLAGS += -msse4.1
  DEFINES += QN_EXPORT=
}

unix:!mac {
    LIBS += -lopenal
}

DEFINES += __STDC_CONSTANT_MACROS

LIBS += -L$$EVETOOLS_DIR/lib

unix {
  LIBS += -L/usr/lib
}

INCLUDEPATH += ../contrib/qtcolorpicker/src
LIBS += -L../contrib/qtcolorpicker/lib
DEFINES += QT_QTCOLORPICKER_IMPORT

CONFIG(debug, debug|release) {
  INCLUDEPATH += $$FFMPEG-debug/include
  LIBS = -L$$FFMPEG-debug/bin -L$$FFMPEG-debug/lib -L$$PWD/../../common/bin/debug -lcommon -L$$EVETOOLS_DIR/lib/debug $$LIBS

  win32 {
      LIBS += -lQtSolutions_ColorPicker-2.6d
  }

  unix {
    LIBS += -L/usr/lib/debug
  }
}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
  LIBS = -L$$FFMPEG-release/bin -L$$FFMPEG-release/lib -L$$PWD/../../common/bin/release -lcommon -L$$EVETOOLS_DIR/lib/release $$LIBS

  win32 {
      LIBS += -lQtSolutions_ColorPicker-2.6
  }

  unix {
    LIBS += -L/usr/lib/release
  }
}

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale

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
  LIBS += -lbz2
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

RESOURCES += ../build/skin.qrc
FORMS += \
    ui/dialogs/preferences_dialog.ui \
    ui/dialogs/login_dialog.ui \
    ui/dialogs/event_log_dialog.ui \
    ui/dialogs/sign_dialog.ui \
    ui/dialogs/tags_edit_dialog.ui \
    ui/dialogs/server_settings_dialog.ui \
    ui/dialogs/layout_name_dialog.ui \
    ui/dialogs/user_settings_dialog.ui \
    ui/dialogs/connection_testing_dialog.ui \
    ui/dialogs/resource_list_dialog.ui \
    ui/dialogs/about_dialog.ui \
    youtube/youtubeuploaddialog.ui \
    youtube/youtubesetting.ui \
    ui/widgets/settings/connections_settings_widget.ui \
    ui/widgets/settings/license_widget.ui \
    ui/widgets/settings/license_manager_widget.ui \
    ui/widgets/settings/recording_settings_widget.ui \
    ui/widgets/help_widget.ui \
    ui/widgets/resource_tree_widget.ui \
    ui/widgets/properties/camera_schedule_widget.ui \
    ui/widgets/properties/single_camera_settings_widget.ui \
    ui/widgets/properties/multiple_camera_settings_widget.ui \

# CL_TRIAL_MODE 
DEFINES += CL_FORCE_LOGO
#DEFINES += CL_CUSTOMIZATION_PRESET=\\\"trinity\\\"
# or
#DEFINES += QN_SKIN_PATH=\\\"./trinity\\\"


# Define override specifier.
OVERRIDE_DEFINITION = "override="
win32-msvc*:OVERRIDE_DEFINITION = "override=override"
DEFINES += $$OVERRIDE_DEFINITION

# Uncomment to enable background color adjustment
# DEFINES += QN_HAS_BACKGROUND_COLOR_ADJUSTMENT

SOURCES += $$PWD/../build/generated/compatibility_info.cpp

mac {
    OBJECTIVE_SOURCES += $$PWD/mac_utils.mm
}
