[settings]
build_type=Release

[options]
boost:zlib = False
boost:bzip2 = False
boost:without_locale = True
boost:without_log = True
boost:without_stacktrace = True
msys2:remove_pkgconf = False

[build_requires]
boost/1.76.0:b2/4.6.1
