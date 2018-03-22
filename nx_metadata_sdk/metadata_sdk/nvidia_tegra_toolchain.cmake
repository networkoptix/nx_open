set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Define before using this toolchain file.
set(TOOLCHAIN_DIR "NOTFOUND")
set(SYSROOT_DIR "NOTFOUND")

set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/gcc-7.2.0/bin/aarch64-unknown-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/gcc-7.2.0/bin/aarch64-unknown-linux-gnu-g++")

set(_sysroot_libs_dir1 "${SYSROOT_DIR}/sysroot/usr/lib/aarch64-linux-gnu")
set(_sysroot_libs_dir2 "${SYSROOT_DIR}/sysroot/usr/lib/aarch64-linux-gnu/tegra")
set(_link_flags "-Wl,-rpath-link,${_sysroot_libs_dir1}:${_sysroot_libs_dir2} -latomic")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_link_flags}")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "" FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
