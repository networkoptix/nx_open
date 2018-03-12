set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "rpi")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-linaro-7.2.1/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-linaro-7.2.1/bin/arm-linux-gnueabihf-g++")

include_directories(SYSTEM "${PACKAGES_DIR}/rpi/sysroot/usr/include")

set(_sysroot_dir "${PACKAGES_DIR}/rpi/sysroot")
set(common_link_flags "-latomic")
string(APPEND common_link_flags " -Wl,-rpath-link,${_sysroot_dir}/lib/arm-linux-gnueabihf")
string(APPEND common_link_flags " -Wl,-rpath-link,${_sysroot_dir}/usr/lib/arm-linux-gnueabihf")
string(APPEND common_link_flags " -L${_sysroot_dir}/lib/arm-linux-gnueabihf")
string(APPEND common_link_flags " -L${_sysroot_dir}/usr/lib/arm-linux-gnueabihf")
unset(_sysroot_dir)

set(CMAKE_EXE_LINKER_FLAGS_INIT "${common_link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${common_link_flags}")

set(common_c_flags "-mcpu=cortex-a7 -mfpu=neon-vfpv4")
string(APPEND common_c_flags " -I${zlib_dir}/include")
set(CMAKE_C_FLAGS_INIT "${common_c_flags}")
set(CMAKE_CXX_FLAGS_INIT "${common_c_flags}")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by rpi.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
