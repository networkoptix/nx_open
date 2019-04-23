set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(cross_prefix ${PACKAGES_DIR}/linux-arm/gcc-8.1/bin/arm-linux-gnueabihf)
set(CMAKE_C_COMPILER "${cross_prefix}-gcc")
set(CMAKE_CXX_COMPILER "${cross_prefix}-g++")

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a7 -mfpu=neon-vfpv4")
set(CMAKE_CXX_FLAGS_INIT ${CMAKE_C_FLAGS_INIT})

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by linux_arm32.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
