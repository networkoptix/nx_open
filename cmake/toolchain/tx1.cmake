set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(box "tx1")

set(cross_prefix ${PACKAGES_DIR}/linux-aarch64/gcc-8.1/bin/aarch64-linux-gnu)
set(CMAKE_C_COMPILER "${cross_prefix}-gcc")
set(CMAKE_CXX_COMPILER "${cross_prefix}-g++")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by tx1.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
