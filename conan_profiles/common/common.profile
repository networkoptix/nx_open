[settings]
build_type=Release
# OpenCV compilation fails with C++20.
opencv/*:compiler.cppstd=17

[options]
boost/*:zlib = False
boost/*:bzip2 = False
boost/*:without_locale = True
boost/*:without_log = True
boost/*:without_stacktrace = True
msys2/*:remove_pkgconf = False
opencv/*:parallel=False
opencv/*:contrib=True
opencv/*:contrib_freetype=False
opencv/*:contrib_sfm=False
opencv/*:with_jpeg=False
opencv/*:with_png=False
opencv/*:with_tiff=False
opencv/*:with_jpeg2000=False
opencv/*:with_openexr=False
opencv/*:with_eigen=False
opencv/*:with_webp=False
opencv/*:with_quirc=False
opencv/*:dnn=True
opencv/*:with_ffmpeg=False
protobuf/*:with_zlib=False

[tool_requires]
boost/*:b2/4.10.1

[buildenv]
# libpq puts the content of this variable directly to CFLAGS. We need to clear it because our CI
# uses this variable for its own needs and we don't want to see its content in CFLAGS.
libpq/*:PROFILE=!
