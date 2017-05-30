set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "isd_s2")

set(_packages_dir "$ENV{environment}/packages/${box}")
set(CMAKE_C_COMPILER "${_packages_dir}/gcc-4.9.3/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${_packages_dir}/gcc-4.9.3/bin/arm-linux-gnueabihf-g++")
unset(_packages_dir)

set(CMAKE_C_FLAGS_INIT "-march=armv7 -mtune=cortex-a9 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")
