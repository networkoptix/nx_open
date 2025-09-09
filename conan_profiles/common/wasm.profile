include(default)
include(common.profile)

[settings]
os=Emscripten
arch=wasm
compiler=clang
compiler.version=16
compiler.libcxx=libc++
compiler.cppstd=20

[options]
openssl/*:shared=False
qt/*:shared=False

qt/*:qtwebengine=False
qt/*:qtwebview=False
qt/*:qtserialport=False

ffmpeg/*:shared=False
ffmpeg/*:av1=False
ffmpeg/*:vorbis=False
ffmpeg/*:vpx=False
ffmpeg/*:openh264=False
ffmpeg/*:mp3lame=False
boost/*:header_only=True

[tool_requires]
emsdk/3.1.72
nodejs/20.16.0

[env]
CFLAGS=-sUSE_PTHREADS=1
CXXFLAGS=-sUSE_PTHREADS=1
LDFLAGS=-sUSE_PTHREADS=1
