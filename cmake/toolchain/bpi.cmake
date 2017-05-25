set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)
set(box bpi)

set(_packages_dir "$ENV{environment}/packages/bpi")

set(CMAKE_C_COMPILER "${_packages_dir}/gcc-4.8.3/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${_packages_dir}/gcc-4.8.3/bin/arm-linux-gnueabihf-g++")

include_directories(SYSTEM "${_packages_dir}/sysroot/usr/include")

set(_sysroot_libs_dir "${_packages_dir}/sysroot/usr/lib/arm-linux-gnueabihf")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir}")
unset(_sysroot_libs_dir)

unset(_packages_dir)
