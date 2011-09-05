QT = core gui network xml opengl multimedia webkit
CONFIG += x86 precompile_header
CONFIG -= flat
TEMPLATE = app
VERSION = 0.0.1
ICON = eve_logo.icns
QMAKE_INFO_PLIST = Info.plist

TARGET = EvePlayer-Beta

include(../contrib/qtsingleapplication/src/qtsingleapplication.pri)

CONFIG(debug, debug|release) {
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
}

mac {
  LIBS += -framework OpenAL
}

CONFIG(debug, debug|release) {
  INCLUDEPATH += $$FFMPEG-debug/include
  LIBS += -L$$FFMPEG-debug/bin -L$$FFMPEG-debug/lib
}
CONFIG(release, debug|release) {
  INCLUDEPATH += $$FFMPEG-release/include
  LIBS += -L$$FFMPEG-release/bin -L$$FFMPEG-release/lib
}

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale

win32 {
  win32-msvc* {
    QMAKE_CXXFLAGS += -MP /Fd$$OBJECTS_DIR
    DEFINES += _CRT_SECURE_NO_WARNINGS
    INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
    INCLUDEPATH += "$$(DXSDK_DIR)/Include"
    !contains(QMAKE_HOST.arch, x86_64) {
      LIBS += -L"$$(DXSDK_DIR)/Lib/x86"
    } else {
      LIBS += -L"$$(DXSDK_DIR)/Lib/x64"
    }
  }

  LIBS += -lws2_32 -lIphlpapi -lOle32

  asm_sources = base/colorspace_convert/colorspace_rgb_mmx.asm
  SOURCES += $$asm_sources base/colorspace_convert/colorspace.c
  DEFINES += ARCH_IS_32BIT ARCH_IS_IA32

  asm_compiler.commands = ..\\contrib\\nasm.exe -f win32 -DWINDOWS ${QMAKE_FILE_IN} -o $$OBJECTS_DIR/${QMAKE_FILE_BASE}.obj -I$$PWD
  asm_compiler.input = asm_sources
  asm_compiler.output = $$OBJECTS_DIR/${QMAKE_FILE_BASE}.obj
  asm_compiler.CONFIG = explicit_dependencies
  compiler.depends=$$OBJECTS_DIR/${QMAKE_FILE_BASE}.obj
  asm_compiler.variable_out = OBJECTS
  QMAKE_EXTRA_COMPILERS += asm_compiler
  LIBS += -ld3d9 -ld3dx9 -lPropsys -lwinmm

  SOURCES += dsp_effects/speex/preprocess.c dsp_effects/speex/filterbank.c dsp_effects/speex/fftwrap.c dsp_effects/speex/smallft.c  dsp_effects/speex/mdf.c
}

mac {
  LIBS += -framework IOKit -framework CoreServices
  LIBS += -lz -lbz2

  PRIVATE_FRAMEWORKS.files = ../resource/arecontvision
  PRIVATE_FRAMEWORKS.path = Contents/MacOS
  QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS

  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f $$PWD/../resource/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = $$PWD/StdAfx.h
PRECOMPILED_SOURCE = $$PWD/StdAfx.cpp

DEFINES += __STDC_CONSTANT_MACROS

RESOURCES += mainwnd.qrc ../build/skin.qrc
FORMS += mainwnd.ui preferences.ui licensekey.ui recordingsettings.ui ui/dialogs/tagseditdialog.ui

DEFINES += CL_TRIAL_MODE
