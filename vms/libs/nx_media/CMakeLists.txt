## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Isolate C/CXX compile options specified on the top level via `add_compile_options` from CUDA
# compile options. CUDA compiler tries to process unknown warning/error options itself and fails.
get_directory_property(compile_options COMPILE_OPTIONS)
list(JOIN compile_options " " compile_options)
string(APPEND CMAKE_C_FLAGS " ${compile_options}")
string(APPEND CMAKE_CXX_FLAGS " ${compile_options}")
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")

find_package(Qt6 COMPONENTS OpenGL)

set(additional_sources)
set(additional_sources_include_dirs)
set(additional_libraries)
set(artifacts_dir ${open_source_root}/artifacts)

# QuickSync/Nvidia: Add additional sources for the supported platforms (Windows x64 and Linux x64).
if(NOT APPLE AND arch STREQUAL "x64")
    set(hardware_decoders_x64 ON)
endif()

if(hardware_decoders_x64)
    enable_language(CUDA)
    find_package(CUDAToolkit)
    find_package(libvpl REQUIRED)

    # Add allocators from artifacts/intel_media_sdk/.
    set(intel_media_sdk_dir ${artifacts_dir}/intel_media_sdk/samples/sample_common)
    list(APPEND additional_sources_include_dirs ${intel_media_sdk_dir}/include)
    list(APPEND additional_sources
        ${intel_media_sdk_dir}/include/base_allocator.h
        ${intel_media_sdk_dir}/include/sysmem_allocator.h
        ${intel_media_sdk_dir}/src/base_allocator.cpp
        ${intel_media_sdk_dir}/src/sysmem_allocator.cpp
    )
    # Add Nvidia decoder sources.
    list(APPEND additional_sources_include_dirs ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Interface)
    list(APPEND additional_sources_include_dirs ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Samples)
    list(APPEND additional_sources
        ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Samples/NvCodec/NvDecoder.cpp
    )

    set(color_conversion_src_path ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Samples/Utils)
    list(APPEND additional_sources
        ${color_conversion_src_path}/ColorSpace.cu
        ${color_conversion_src_path}/Resize.cu
    )
    list(APPEND additional_libraries CUDA::cudart)

    if(LINUX)
        list(APPEND additional_sources
            ${intel_media_sdk_dir}/include/vaapi_allocator.h
            ${intel_media_sdk_dir}/src/vaapi_allocator.cpp
        )
    endif()
    if(WINDOWS)
        list(APPEND additional_sources
            ${intel_media_sdk_dir}/include/d3d_allocator.h
            ${intel_media_sdk_dir}/src/d3d_allocator.cpp
        )
    endif()

    # Add sources from artifacts/libva/va/.
    if(LINUX)
        set(libva_src_dir ${artifacts_dir}/libva/va)

        file(GLOB_RECURSE libva_files CONFIGURE_DEPENDS
            ${libva_src_dir}/*.h
            ${libva_src_dir}/*.cpp
        )

        list(APPEND additional_sources ${libva_files})
        list(APPEND additional_sources_include_dirs ${libva_src_dir})
    endif()
endif()

nx_add_target(nx_media
    LIBRARY
    GDI
    PUBLIC_LIBS
        nx_kit
        nx_audio
        nx_vms_common
        nx_fusion
        nx_utils
        ${additional_libraries}
    PRIVATE_LIBS
        Qt6::OpenGL
    ADDITIONAL_SOURCES
        ${additional_sources}
    FOLDER client/libs
)

target_compile_definitions(nx_media
    PRIVATE NX_MEDIA_API=${API_EXPORT_MACRO}
    INTERFACE NX_MEDIA_API=${API_IMPORT_MACRO}
)

target_include_directories(nx_media
    PRIVATE
        ${Qt6Multimedia_PRIVATE_INCLUDE_DIRS}
        ${additional_sources_include_dirs}
)

if(ANDROID OR IOS)
    find_package(libjpeg-turbo REQUIRED)
    target_link_libraries(nx_media PRIVATE libjpeg-turbo::libjpeg-turbo)
endif()

if(hardware_decoders_x64)
    if(LINUX)
        find_package(libva REQUIRED)
        find_package(intel-onevpl REQUIRED)
        find_package(intel-media-sdk REQUIRED)
        target_link_libraries(nx_media
            PUBLIC
            X11 dl GL libva::libva libvpl::vpl intel-onevpl::mfx-gen
        )
    endif()

    if(WINDOWS)
        include("${CONAN_INTEL-MEDIA-SDK-BIN_ROOT}/intel_media_sdk.cmake")
        target_link_libraries(nx_media PUBLIC opengl32 dxva2 d3d9 intel-media-sdk libvpl::vpl)
    endif()
else()
    nx_exclude_sources_from_target(nx_media src/nx/media/nvidia)
    nx_exclude_sources_from_target(nx_media src/nx/media/quick_sync)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
