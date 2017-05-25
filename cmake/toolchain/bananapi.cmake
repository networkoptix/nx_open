set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(box "bananapi")

set(_packages_dir "$ENV{environment}/packages/bpi")
set(CMAKE_C_COMPILER "${_packages_dir}/gcc-4.8.3/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${_packages_dir}/gcc-4.8.3/bin/arm-linux-gnueabihf-g++")
unset(_packages_dir)
