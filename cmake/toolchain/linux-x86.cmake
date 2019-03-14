set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(cross_prefix ${PACKAGES_DIR}/linux-x86/gcc-8.1/bin/i686-pc-linux-gnu)
set(CMAKE_C_COMPILER "${cross_prefix}-gcc")
set(CMAKE_CXX_COMPILER "${cross_prefix}-g++")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by linux-x86.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
