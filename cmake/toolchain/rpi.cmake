set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "rpi")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-7.2.0/bin/arm-unknown-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/linux-arm/gcc-7.2.0/bin/arm-unknown-linux-gnueabihf-g++")

set(_glib_dir "${PACKAGES_DIR}/linux-arm/glib-2.0")
set(common_link_flags "-Wl,-rpath-link,${_glib_dir}/lib -latomic")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${common_link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${common_link_flags}")
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a7 -mfpu=neon-vfpv4")
set(CMAKE_CXX_FLAGS_INIT ${CMAKE_C_FLAGS_INIT})
unset(_glib_dir)

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by rpi.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
