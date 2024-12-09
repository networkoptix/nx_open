[settings]
build_type=Release
intel-media-driver/*:compiler.cppstd=17
libdatachannel/*:build_type=RelWithDebInfo

[options]
boost/*:zlib = False
boost/*:bzip2 = False
boost/*:without_locale = True
boost/*:without_log = True
boost/*:without_stacktrace = True
icu/*:shared=True
libsrtp/*:with_openssl = True
opencv*:dnn=True
opencv*:freetype=False
opencv*:gapi=False
opencv*:sfm=False
opencv*:with_eigen=False
opencv*:with_ffmpeg=False
opencv*:with_jpeg2000=False
opencv*:with_openexr=False
opencv*:with_png=False
opencv*:with_protobuf=False
opencv*:with_quirc=False
opencv*:with_tesseract=False
opencv*:with_tiff=False
opencv*:with_wayland=False
opencv*:with_webp=False
opencv-static/*:with_jpeg=libjpeg-turbo
opencv/*:shared=True
opencv/*:with_cuda=False
opencv/*:with_jpeg=False
prometheus-cpp/*:with_pull=False
prometheus-cpp/*:with_push=False
sdk-gcc/*:with_sanitizer=False

[tool_requires]
boost/*:b2/4.10.1

[buildenv]
# libpq puts the content of this variable directly to CFLAGS. We need to clear it because our CI
# uses this variable for its own needs and we don't want to see its content in CFLAGS.
libpq/*:PROFILE=!

[conf]
qt/*:tools.cmake.cmaketoolchain:generator=Ninja
