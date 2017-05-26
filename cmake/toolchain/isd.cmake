set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "isd")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/${box}/gcc-4.9.3/bin/arm-linux-gnueabi-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/${box}/gcc-4.9.3/bin/arm-linux-gnueabi-g++")

set(CMAKE_C_FLAGS_INIT "-march=armv6")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")
