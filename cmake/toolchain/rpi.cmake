set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "rpi")

set(CMAKE_C_COMPILER "${PACKAGES_DIR}/${box}/gcc-4.9.4/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${PACKAGES_DIR}/${box}/gcc-4.9.4/bin/arm-linux-gnueabihf-g++")
