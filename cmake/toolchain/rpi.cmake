set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "rpi")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/${box}/gcc-4.9.3/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/${box}/gcc-4.9.3/bin/arm-linux-gnueabihf-g++")

include_directories(SYSTEM "${PACKAGES_DIR}/${box}/sysroot/usr/include")

set(_sysroot_libs_dir "${PACKAGES_DIR}/${box}/sysroot/usr/lib/arm-linux-gnueabihf")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir}")
unset(_sysroot_libs_dir)
