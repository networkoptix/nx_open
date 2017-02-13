set(CMAKE_SYSTEM_NAME Android)

set(CMAKE_ANDROID_STANDALONE_TOOLCHAIN $ENV{environment}/packages/android-arm/toolchain-r13b)
set(CMAKE_ANDROID_ARM_MODE ON)
set(CMAKE_ANDROID_ARCH_ABI armeabi-v7a)
set(CMAKE_ANDROID_STL_TYPE gnustl_shared)
