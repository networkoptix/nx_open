set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/linux-x86/gcc-7.3.0/bin/i686-pc-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/linux-x86/gcc-7.3.0/bin/i686-pc-linux-gnu-g++")

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by linux-x86.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
