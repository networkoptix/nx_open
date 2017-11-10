set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(box "tx1")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/${box}/gcc-5.4.1/bin/aarch64-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/${box}/gcc-5.4.1/bin/aarch64-linux-gnu-g++")

set(_sysroot_libs_dir1 "${PACKAGES_DIR}/${box}/sysroot/usr/lib/aarch64-linux-gnu")
set(_sysroot_libs_dir2 "${PACKAGES_DIR}/${box}/sysroot/usr/lib/aarch64-linux-gnu/tegra")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir1}:${_sysroot_libs_dir2}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,-rpath-link,${_sysroot_libs_dir1}:${_sysroot_libs_dir2}")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by tx1.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
