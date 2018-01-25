set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "rpi")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-7.2.0/bin/arm-unknown-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-7.2.0/bin/arm-unknown-linux-gnueabihf-g++")

include_directories(SYSTEM "${PACKAGES_DIR}/${box}/sysroot/usr/include")
set(_sysroot_libs_dir "${PACKAGES_DIR}/${box}/sysroot/usr/lib/arm-linux-gnueabihf")

set(glib_dir "${PACKAGES_DIR}/linux-arm/glib-2.0")
set(zlib_dir "${PACKAGES_DIR}/linux-arm/zlib-1.2")
set(common_link_flags "-Wl,-rpath-link,${glib_dir}/lib")
string(APPEND common_link_flags " -L${zlib_dir}/lib -Wl,-rpath-link,${zlib_dir}/lib")
string(APPEND common_link_flags " -latomic")
string(APPEND common_link_flags " -Wl,-rpath-link,${_sysroot_libs_dir} -L${_sysroot_libs_dir}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${common_link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${common_link_flags}")

set(common_c_flags "-mcpu=cortex-a7 -mfpu=neon-vfpv4")
string(APPEND common_c_flags " -I${zlib_dir}/include")
set(CMAKE_C_FLAGS_INIT "${common_c_flags}")
set(CMAKE_CXX_FLAGS_INIT "${common_c_flags}")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by rpi.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)

unset(_sysroot_libs_dir)
