set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "edge1")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-7.2.0/bin/arm-unknown-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-7.2.0/bin/arm-unknown-linux-gnueabihf-g++")

set(glib_dir "${PACKAGES_DIR}/linux-arm/glib-2.0")
set(zlib_dir "${PACKAGES_DIR}/linux-arm/zlib-1.2")
set(common_link_flags "-Wl,-rpath-link,${glib_dir}/lib")
string(APPEND common_link_flags " -L${zlib_dir}/lib -Wl,-rpath-link,${zlib_dir}/lib")
string(APPEND common_link_flags " -latomic")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${common_link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${common_link_flags}")

set(common_c_flags "-mcpu=cortex-a9 -mfpu=vfpv3 -mfpu=neon")
string(APPEND common_c_flags " -I${zlib_dir}/include")
set(CMAKE_C_FLAGS_INIT "${common_c_flags}")
set(CMAKE_CXX_FLAGS_INIT "${common_c_flags}")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by edge1.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
