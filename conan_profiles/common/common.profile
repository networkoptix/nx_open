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

[env]
# libpq puts the content of this variable directly to CFLAGS. We need to clear it because our CI
# uses this variable for its own needs and we don't want to see its content in CFLAGS.
libpq:PROFILE =
