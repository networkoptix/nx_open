set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(cross_prefix ${PACKAGES_DIR}/linux_arm32/gcc-5.4.0/bin/arm-linux-gnueabihf)
set(CMAKE_C_COMPILER "${cross_prefix}-gcc")
set(CMAKE_CXX_COMPILER "${cross_prefix}-g++")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "" FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
