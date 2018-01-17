set(CMAKE_SYSTEM_NAME Android)
set(box "android")

if(NOT "$ENV{ANDROID_NDK}" STREQUAL "")
    set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK}")
else()
    set(CMAKE_ANDROID_NDK "${PACKAGES_DIR}/android/android-ndk-r16")
endif()

if(NOT $ENV{ANDROID_HOME} STREQUAL "")
    set(ANDROID_SDK $ENV{ANDROID_HOME})
else()
    set(ANDROID_SDK "${PACKAGES_DIR}/android/android-sdk")
endif()

set(CMAKE_SYSTEM_VERSION 16)
set(CMAKE_ANDROID_ARM_MODE ON)
set(CMAKE_ANDROID_ARCH_ABI armeabi-v7a)
set(CMAKE_ANDROID_STL_TYPE gnustl_shared)
set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)

# These macros are not required because they are declared by a compiler.
# We declare them explicitly to improve code parsing by IDEs.
add_definitions(-DANDROID -D__ANDROID__)
