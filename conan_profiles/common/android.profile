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
openal/*:AndroidNDK/r26d
qt/*:AndroidNDK/r26d, AndroidSDK/33, openjdk/18.0.1
openssl/*:AndroidNDK/r26d
ffmpeg/*:AndroidStandaloneToolchain/r26d
libjpeg-turbo/*:AndroidNDK/r26d
pathkit/*:AndroidNDK/r26d
