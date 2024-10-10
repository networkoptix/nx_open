include(default)
include(common.profile)

[settings]
os=iOS
os.version=16.0
os.sdk=iphoneos
compiler=apple-clang
compiler.libcxx=libc++
compiler.cppstd=20

[options]
zlib/*:shared=False
openssl/*:shared=False
qt/*:shared=False
qt/*:qtwebengine=False
ffmpeg:shared=False
ffmpeg/*:vorbis=False
ffmpeg/*:vpx=False
ffmpeg/*:mp3lame=False
ffmpeg/*:openh264=False
boost/*:header_only = True
