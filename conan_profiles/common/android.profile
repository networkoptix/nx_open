include(default)
include(common.profile)

[settings]
compiler=clang
compiler.version=17
compiler.libcxx=c++_shared
compiler.cppstd=20

os=Android
os.api_level=22

[options]
qt/*:qtwebengine=False
qt/*:qtserialport=False
ffmpeg/*:vorbis=False
ffmpeg/*:vpx=False
ffmpeg/*:mp3lame=False
ffmpeg/*:openh264=False
boost/*:header_only = True

[tool_requires]
AndroidNDK/r26d
ffmpeg/*:AndroidStandaloneToolchain/r26d
qt/*:AndroidSDK/34, openjdk/18.0.1
