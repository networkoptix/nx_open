include(default)
include(common.profile)

[settings]
compiler=clang
compiler.version=21
compiler.libcxx=c++_shared
compiler.cppstd=20
build_type=RelWithDebInfo

os=Android
os.api_level=28

[options]
qt/*:qtwebengine=False
qt/*:qtserialport=False
ffmpeg/*:vorbis=False
ffmpeg/*:vpx=False
ffmpeg/*:openh264=False
ffmpeg/*:av1=False
boost/*:header_only = True

[tool_requires]
*:android-ndk/r29
ffmpeg/*:android-standalone-toolchain/r29
libmp3lame/*:android-standalone-toolchain/r29
qt/*:android-sdk/36, openjdk/21.0.2
