set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "bananapi")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-7.2.0/bin/arm-unknown-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-7.2.0/bin/arm-unknown-linux-gnueabihf-g++")

include_directories(SYSTEM "${PACKAGES_DIR}/bpi/sysroot/usr/include")

set(_sysroot_libs_dir "${PACKAGES_DIR}/bpi/sysroot/usr/lib/arm-linux-gnueabihf")
set(common_link_flags "-Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir} -latomic")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${common_link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${common_link_flags}")
unset(_sysroot_libs_dir)

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by tx1.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
