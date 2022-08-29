## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(additional_sources)
set(additional_sources_include_dirs)

set(artifacts_dir ${open_source_root}/artifacts)

list(APPEND additional_sources_include_dirs ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Interface)
list(APPEND additional_sources_include_dirs ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Samples)

# QuickSync: Add additional sources for the supported platforms (Windows x64 and Linux x64).
if(NOT APPLE AND arch STREQUAL "x64")
    # Add allocators from artifacts/intel_media_sdk/.
    set(intel_media_sdk_dir ${artifacts_dir}/intel_media_sdk/samples/sample_common)
    list(APPEND additional_sources_include_dirs ${intel_media_sdk_dir}/include)
    list(APPEND additional_sources
        ${intel_media_sdk_dir}/include/base_allocator.h
        ${intel_media_sdk_dir}/include/sysmem_allocator.h
        ${intel_media_sdk_dir}/src/base_allocator.cpp
        ${intel_media_sdk_dir}/src/sysmem_allocator.cpp
    )
    # Add Nvidia decoder sources
    list(APPEND additional_sources
        ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Samples/NvCodec/NvDecoder.cpp
    )

    if(LINUX)
        list(APPEND additional_sources
            ${intel_media_sdk_dir}/include/vaapi_allocator.h
            ${intel_media_sdk_dir}/src/vaapi_allocator.cpp
        )

        set(libcolor_conversion_path ${CMAKE_BINARY_DIR}/lib/libcolor_conversion.so)
        set(cudart_library_path ${CMAKE_BINARY_DIR}/lib/libcudart.so)
        set(libcolor_conversion_src_path ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Samples/Utils)
        set(NVCC_COMMAND ${CONAN_CUDA-TOOLKIT_ROOT}/cuda-toolkit/usr/local/cuda-11.7/bin/nvcc
            --compiler-bindir ${CMAKE_CXX_COMPILER}
            --shared -o ${libcolor_conversion_path}
            ${libcolor_conversion_src_path}/ColorSpace.cu
            ${libcolor_conversion_src_path}/Resize.cu
            -gencode arch=compute_50,code=\\"sm_50,compute_50\\" -std=c++11  --compiler-options -fPIC)
        add_custom_command(
            OUTPUT ${libcolor_conversion_path}
            DEPENDS
                ${libcolor_conversion_src_path}/ColorSpace.cu
                ${libcolor_conversion_src_path}/Resize.cu
                ${libcolor_conversion_src_path}/ColorSpace.h
            COMMAND ${NVCC_COMMAND}
        )
        add_custom_target(libcolor_conversion DEPENDS ${libcolor_conversion_path})

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
    NO_API_MACROS
    GDI
    PUBLIC_LIBS
        nx_kit
        nx_audio
        nx_vms_common
        nx_fusion
        nx_utils
        ${cudart_library_path}
        ${libcolor_conversion_path}
    ADDITIONAL_SOURCES
        ${additional_sources}
    FOLDER client/libs
)

if(LINUX AND arch STREQUAL "x64")
    add_dependencies(nx_media libcolor_conversion)
endif()

target_include_directories(nx_media
    PRIVATE
        ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS}
        ${additional_sources_include_dirs}
        ${CONAN_CUDA-TOOLKIT_ROOT}/cuda-toolkit/usr/local/cuda-11.7/include
)

if(ANDROID OR IOS)
    find_package(libjpeg-turbo REQUIRED)
    target_link_libraries(nx_media PRIVATE libjpeg-turbo::libjpeg-turbo)
endif()

if(APPLE OR WINDOWS OR NOT arch STREQUAL "x64")
    nx_exclude_sources_from_target(nx_media src/nx/media/nvidia)
endif()

if(APPLE OR NOT arch STREQUAL "x64")
    nx_exclude_sources_from_target(nx_media src/nx/media/quick_sync)

else()
    if(LINUX)
        find_package(libva REQUIRED)
        find_package(intel-media-sdk REQUIRED)
        target_link_libraries(nx_media
            PUBLIC
            X11 dl GL libva::libva intel-media-sdk::intel-media-sdk
        )
    endif()

    if(WINDOWS)
        include("${CONAN_INTEL-MEDIA-SDK-BIN_ROOT}/intel_media_sdk.cmake")
        target_link_libraries(nx_media PUBLIC opengl32 dxva2 d3d9 intel-media-sdk)
    endif()
endif()

if(ANDROID)
    target_link_libraries(nx_media PRIVATE Qt5::AndroidExtras)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
