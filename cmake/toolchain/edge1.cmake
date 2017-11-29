set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "edge1")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/isd_s2/gcc-4.9.3/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/isd_s2/gcc-4.9.3/bin/arm-linux-gnueabihf-g++")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-O1")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-O1")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-O1")
set(CMAKE_C_FLAGS_INIT "-march=armv7 -mtune=cortex-a9 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")
