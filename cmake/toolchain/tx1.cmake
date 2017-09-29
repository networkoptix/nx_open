set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(box "tx1")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/${box}/gcc-5.4.1/bin/aarch64-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/${box}/gcc-5.4.1/bin/aarch64-linux-gnu-g++")

include_directories(SYSTEM "${PACKAGES_DIR}/${box}/sysroot/usr/include")

set(_sysroot_libs_dir "${PACKAGES_DIR}/${box}/sysroot/usr/lib/aarch64-linux-gnu")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir}")
unset(_sysroot_libs_dir)