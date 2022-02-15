include(common/macos.profile)

[settings]
os.version=11.0
arch=armv8

[env]
libvpx:CFLAGS=
libvpx:CXXFLAGS=
libvpx:LDFLAGS=
CFLAGS=-target arm64-apple-macos11
CXXFLAGS=-target arm64-apple-macos11
LDFLAGS=-target arm64-apple-macos11
