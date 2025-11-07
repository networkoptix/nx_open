include(default)
include(common.profile)

[settings]
compiler=clang
compiler.version=21
compiler.libcxx=c++_shared
compiler.cppstd=20

os=Android
os.api_level=26

[options]
qt/*:qtwebengine=False
qt/*:qtserialport=False
ffmpeg/*:vorbis=False
ffmpeg/*:vpx=False
ffmpeg/*:openh264=False
ffmpeg/*:av1=False
boost/*:header_only = True

[tool_requires]
AndroidNDK/r29
ffmpeg/*:AndroidStandaloneToolchain/r29
libmp3lame/*:AndroidStandaloneToolchain/r29
qt/*:AndroidSDK/34, openjdk/18.0.1
