set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER ${PACKAGES_DIR}/linux/clang-6.0.0/bin/clang)
set(CMAKE_CXX_COMPILER ${PACKAGES_DIR}/linux/clang-6.0.0/bin/clang++)

set(flags "--gcc-toolchain=${PACKAGES_DIR}/linux-x64/gcc-8.1")
string(APPEND flags " --sysroot=${PACKAGES_DIR}/linux-x64/gcc-8.1/x86_64-pc-linux-gnu/sysroot")
set(CMAKE_C_FLAGS_INIT ${flags})
set(CMAKE_CXX_FLAGS_INIT ${flags})

# This is required by FindThreads CMake module.
set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by linux-x64-clang.cmake." FORCE)
mark_as_advanced(THREADS_PTHREAD_ARG)
