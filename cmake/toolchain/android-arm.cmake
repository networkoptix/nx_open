set(CMAKE_SYSTEM_NAME Android)

if(NOT $ENV{ANDROID_NDK} STREQUAL "")
    set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK})
else()
    set(CMAKE_ANDROID_NDK $ENV{environment}/packages/android/android-ndk)
endif()

set(CMAKE_SYSTEM_VERSION 16)
set(CMAKE_ANDROID_ARM_MODE ON)
set(CMAKE_ANDROID_ARCH_ABI armeabi-v7a)
set(CMAKE_ANDROID_STL_TYPE gnustl_shared)
