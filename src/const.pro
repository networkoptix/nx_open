QT = core gui network xml xmlpatterns opengl multimedia webkit 
CONFIG += x86 precompile_header
TEMPLATE = app
VERSION = 0.0.1
ICON = eve_logo.icns
QMAKE_INFO_PLIST = Info.plist

TARGET = EvePlayer-Beta

include(../contrib/qtsingleapplication/src/qtsingleapplication.pri)

debug {
  DESTDIR = ../bin/debug
  OBJECTS_DIR  = ../build/debug
  MOC_DIR = ../build/debug/generated
  UI_DIR = ../build/debug/generated
  RCC_DIR = ../build/debug/generated
  INCLUDEPATH += $$FFMPEG-debug/include
}

release {
  DESTDIR = ../bin/release
  OBJECTS_DIR  = ../build/release
  MOC_DIR = ../build/release/generated
  UI_DIR = ../build/release/generated
  RCC_DIR = ../build/release/generated
  INCLUDEPATH += $$FFMPEG-release/include
}


win32 {
  QMAKE_CXXFLAGS += -MP /Fd$(IntDir)
  INCLUDEPATH += ../contrib/ffmpeg-misc-headers-win32
  INCLUDEPATH += ../contrib/openal/include
  RC_FILE = uniclient.rc
}

mac {
  PRIVATE_FRAMEWORKS.files = ../resource/arecontvision
  PRIVATE_FRAMEWORKS.path = Contents/MacOS
  QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS
}

INCLUDEPATH += $$PWD
PRECOMPILED_HEADER = StdAfx.h
PRECOMPILED_SOURCE = StdAfx.cpp

DEFINES += __STDC_CONSTANT_MACROS

RESOURCES += mainwnd.qrc ../build/skin.qrc
FORMS += mainwnd.ui preferences.ui licensekey.ui recordingsettings.ui


win32 {
  LIBS += ws2_32.lib Iphlpapi.lib
  #QMAKE_LFLAGS += avcodec-53.lib avdevice-53.lib avfilter-2.lib avformat-53.lib avutil-51.lib swscale-0.lib
  LIBS += avcodec.lib avdevice.lib avfilter.lib avformat.lib avutil.lib swscale.lib
  QMAKE_LFLAGS_DEBUG += /libpath:$$FFMPEG-debug/bin
  QMAKE_LFLAGS_RELEASE += /libpath:$$FFMPEG-release/bin

  LIBS += ../contrib/openal/bin/win32/OpenAL32.lib

  asm_sources = base/colorspace_convert/colorspace_rgb_mmx.asm
  SOURCES += $$asm_sources base/colorspace_convert/colorspace.c
  DEFINES += ARCH_IS_32BIT ARCH_IS_IA32

  asm_compiler.commands = ..\\contrib\\nasm.exe -f win32 -DWINDOWS ${QMAKE_FILE_IN} -o \$(IntDir)${QMAKE_FILE_BASE}.obj -I$$PWD/
  asm_compiler.input = asm_sources
  asm_compiler.output = \$(IntDir)${QMAKE_FILE_BASE}.obj
  asm_compiler.CONFIG = explicit_dependencies
  compiler.depends=\$(IntDir)${QMAKE_FILE_BASE}.obj
  asm_compiler.variable_out = OBJECTS
  QMAKE_EXTRA_COMPILERS += asm_compiler
  LIBS += d3d9.lib d3dx9.lib dwmapi.lib
}

mac {
  LIBS += -framework SystemConfiguration
  LIBS += -framework OpenAL
  LIBS += -framework IOKit
  QMAKE_LFLAGS += -lavcodec.53 -lavdevice.53 -lavfilter.2 -lavformat.53 -lavutil.51 -lswscale.0 -lz -lbz2
  QMAKE_LFLAGS_DEBUG += -L$$FFMPEG-debug/lib
  QMAKE_LFLAGS_RELEASE += -L$$FFMPEG-release/lib
  QMAKE_POST_LINK += mkdir -p `dirname $(TARGET)`/arecontvision; cp -f $$PWD/../resource/arecontvision/devices.xml `dirname $(TARGET)`/arecontvision
}
