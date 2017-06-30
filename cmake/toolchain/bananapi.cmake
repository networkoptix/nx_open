set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "bananapi")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/bpi/gcc-4.8.3/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/bpi/gcc-4.8.3/bin/arm-linux-gnueabihf-g++")

include_directories(SYSTEM "${PACKAGES_DIR}/bpi/sysroot/usr/include")

set(_sysroot_libs_dir "${PACKAGES_DIR}/bpi/sysroot/usr/lib/arm-linux-gnueabihf")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir}")
unset(_sysroot_libs_dir)
