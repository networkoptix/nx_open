## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(${CMAKE_CURRENT_LIST_DIR}/conan_utils.cmake)

set(_additional_conan_parameters)

# Get dependencies.
set(conan_profile ${targetDevice})
if(targetDevice STREQUAL edge1)
    set(conan_profile linux_arm32)
elseif(targetDevice MATCHES "^windows")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(conan_profile "${targetDevice}_debug")
    else()
        set(conan_profile "${targetDevice}_release")
    endif()
endif()

set(use_clang ${useClang})
nx_to_python_compatible_bool(use_clang)
list(APPEND _additional_conan_parameters "-o useClang=${use_clang}")

list(APPEND _additional_conan_parameters "-o targetDevice=${targetDevice}")

if(NOT developerBuild)
    list(APPEND _additional_conan_parameters "-o installRuntimeDependencies=False")
endif()

if(targetDevice MATCHES "windows")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(build_type "Debug")
    else()
        set(build_type "RelWithDebInfo")
    endif()
else()
    set(build_type "Release")
endif()

if(customizationPackageFile)
    list(APPEND _additional_conan_parameters "-o skipCustomizationPackage=True")
elseif(NOT "${customization}" STREQUAL "")
    list(APPEND _additional_conan_parameters "-o customization=${customization}")
endif()

if(targetDevice MATCHES "^linux|^edge")
    list(APPEND _additional_conan_parameters
        "--profile:host ${open_source_root}/cmake/conan_profiles/gcc.profile"
    )
elseif(targetDevice MATCHES "macos")
    list(APPEND _additional_conan_parameters
        "--profile:build ${open_source_root}/cmake/conan_profiles/xcode.profile"
        "--profile:host ${open_source_root}/cmake/conan_profiles/xcode.profile"
    )
endif()

nx_run_conan(
    BUILD_TYPE ${build_type}
    PROFILE ${CMAKE_SOURCE_DIR}/conan_profiles/${conan_profile}.profile
    FLAGS ${_additional_conan_parameters}
)

set(CONAN_DISABLE_CHECK_COMPILER ON)

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

nx_check_package_paths_changes(conan ${CONAN_DEPENDENCIES})

# Load dependencies.
if(CONAN_GCC-TOOLCHAIN_ROOT)
    set(CONAN_COMPILER gcc)
    set(ENV{TOOLCHAIN_DIR} ${CONAN_GCC-TOOLCHAIN_ROOT})
endif()

if(CONAN_CLANG_ROOT)
    set(ENV{CLANG_DIR} ${CONAN_CLANG_ROOT})
endif()

if(CONAN_ANDROIDSDK_ROOT)
    set(ENV{ANDROID_HOME} ${CONAN_ANDROIDSDK_ROOT})
endif()

if(CONAN_ANDROIDNDK_ROOT)
    set(ENV{ANDROID_NDK} ${CONAN_ANDROIDNDK_ROOT})
endif()

if(CONAN_ANDROIDSDK_ROOT)
    set(ENV{ANDROID_HOME} ${CONAN_ANDROIDSDK_ROOT})
endif()

if(CONAN_QT_ROOT)
    set(QT_DIR ${CONAN_QT_ROOT})
endif()

if(CONAN_MSVC-REDIST_ROOT)
    set(vcrt_directory ${CONAN_MSVC-REDIST_ROOT})
endif()

if(CONAN_DIRECTX_ROOT)
    include(${CONAN_DIRECTX_ROOT}/directx.cmake)
endif()

if(CONAN_WINSDK-REDIST_ROOT)
    set(ucrt_directory ${CONAN_WINSDK-REDIST_ROOT})
endif()

if(CONAN_CUDA-TOOLKIT_ROOT)
    set(CUDAToolkit_ROOT ${CONAN_CUDA-TOOLKIT_ROOT})
    set(ENV{CUDA_PATH} ${CONAN_CUDA-TOOLKIT_ROOT})
endif()

if(CONAN_ICU-WIN_ROOT)
    set(icu_directory ${CONAN_ICU-WIN_ROOT})
endif()

if(CONAN_VMS_HELP_ROOT)
    set(help_directory ${CONAN_VMS_HELP_ROOT})
endif()

if(CONAN_MOBILE_USER_MANUAL_ROOT)
    set(mobile_help_directory ${CONAN_MOBILE_USER_MANUAL_ROOT})
endif()

if(CONAN_QUICK_START_GUIDE_ROOT)
    set(quick_start_guide_directory ${CONAN_QUICK_START_GUIDE_ROOT})
endif()

if(CONAN_NX_MOBILE_CLIENT_DEPENDENCIES_ROOT)
    set(firebase_cpp_sdk_dir ${CONAN_NX_MOBILE_CLIENT_DEPENDENCIES_ROOT}/firebase_cpp_sdk)
    set(push_dependencies_dir ${CONAN_NX_MOBILE_CLIENT_DEPENDENCIES_ROOT}/push_dependencies)
endif()

# Exclude conan-generated files from ninja_clean
foreach(extension "bat" "sh" "ps1")
    nx_store_known_file(${CMAKE_BINARY_DIR}/activate_run.${extension})
    nx_store_known_file(${CMAKE_BINARY_DIR}/deactivate_run.${extension})
    nx_store_known_file(${CMAKE_BINARY_DIR}/environment_run.${extension}.env)
endforeach(extension)

nx_store_known_file(${CMAKE_BINARY_DIR}/lib/ffmpeg)
