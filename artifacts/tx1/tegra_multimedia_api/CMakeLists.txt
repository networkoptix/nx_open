cmake_minimum_required(VERSION 3.9.0)
project(tegra_video)

# ATTENTION: This cmake file is not intended for building the real libtegra_video.so, which is
# to be built via make on a physical Tegra device - it allows only to build a stub version of
# libtegra_video.so and run those unit tests (on a PC) which do not depend on NVidia libraries.
# Sources of the real libtegra_video.so implementation are included here for IDE convenience.

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNX_KIT_API= -DTEGRA_VIDEO_API= -DTEGRA_VIDEO_STUB_ONLY=")

set(NX_KIT_DIR "../../../open/artifacts/nx_kit/src")
set(NX_KIT_SRC_DIR "${NX_KIT_DIR}/nx/kit")

include_directories(
    .
    include
    samples/common/algorithm/gie
    samples/common/algorithm/cuda
    ../../../open/artifacts/nx_kit/src
)

set(TEGRA_VIDEO_STUB_SOURCE_FILES
    ${NX_KIT_SRC_DIR}/debug.h
    ${NX_KIT_SRC_DIR}/debug.cpp
    ${NX_KIT_SRC_DIR}/ini_config.h
    ${NX_KIT_SRC_DIR}/ini_config.cpp
    samples/04_video_dec_gie/tegra_video_ini.h
    samples/04_video_dec_gie/tegra_video.h
    samples/04_video_dec_gie/tegra_video.cpp
    samples/04_video_dec_gie/tegra_video_stub.cpp
)

set(TEGRA_VIDEO_STUB_UT_SOURCE_FILES
    ${TEGRA_VIDEO_STUB_SOURCE_FILES}
    ${NX_KIT_SRC_DIR}/test.h
    ${NX_KIT_SRC_DIR}/test.cpp
    samples/04_video_dec_gie/tegra_video_stub_ut.cpp
)

set(NET_DIMENSIONS_UT_SOURCE_FILES
    ${NX_KIT_SRC_DIR}/debug.h
    ${NX_KIT_SRC_DIR}/debug.cpp
    ${NX_KIT_SRC_DIR}/test.h
    ${NX_KIT_SRC_DIR}/test.cpp
    samples/04_video_dec_gie/net_dimensions.h
    samples/04_video_dec_gie/net_dimensions.cpp
    samples/04_video_dec_gie/net_dimensions_ut.cpp
)

set(SOURCE_FILES
    ${TEGRA_VIDEO_STUB_SOURCE_FILES}
    ${TEGRA_VIDEO_STUB_UT_SOURCE_FILES}
    ${NET_DIMENSIONS_UT_SOURCE_FILES}
    samples/04_video_dec_gie/video_dec_gie.h
    samples/04_video_dec_gie/video_dec_gie_csvparser.cpp
    samples/04_video_dec_gie/video_dec_gie_main.cpp
    samples/04_video_dec_gie/video_dec_gie_main.h
    samples/04_video_dec_gie/video_dec_gie_ini.h
    samples/04_video_dec_gie/tegra_video_impl.cpp
    samples/04_video_dec_gie/main_tv.h
    samples/04_video_dec_gie/main_tv.cpp
    include/Argus/Ext/BayerAverageMap.h
    include/Argus/Ext/BayerSharpnessMap.h
    include/Argus/Ext/DebugCaptureMetadata.h
    include/Argus/Ext/DebugCaptureSession.h
    include/Argus/Ext/DeFog.h
    include/Argus/Ext/FaceDetect.h
    include/Argus/Ext/SensorPrivateMetadata.h
    include/Argus/Argus.h
    include/Argus/CameraDevice.h
    include/Argus/CameraProvider.h
    include/Argus/CaptureMetadata.h
    include/Argus/CaptureSession.h
    include/Argus/Event.h
    include/Argus/EventProvider.h
    include/Argus/EventQueue.h
    include/Argus/Request.h
    include/Argus/Settings.h
    include/Argus/Stream.h
    include/Argus/Types.h
    include/Argus/UUID.h
    include/EGL/egl.h
    include/EGL/eglext.h
    include/EGL/eglext_nv.h
    include/EGL/eglplatform.h
    include/EGLStream/NV/ImageNativeBuffer.h
    include/EGLStream/ArgusCaptureMetadata.h
    include/EGLStream/EGLStream.h
    include/EGLStream/Frame.h
    include/EGLStream/FrameConsumer.h
    include/EGLStream/Image.h
    include/EGLStream/MetadataContainer.h
    include/GL/gl.h
    include/GL/glcorearb.h
    include/GL/glext.h
    include/GL/glxext.h
    include/GLES2/gl2.h
    include/GLES2/gl2ext.h
    include/GLES2/gl2ext_nv.h
    include/GLES2/gl2platform.h
    include/GLES3/gl3.h
    include/GLES3/gl31.h
    include/GLES3/gl32.h
    include/GLES3/gl3platform.h
    include/KHR/khrplatform.h
    include/libjpeg-8b/jconfig.h
    include/libjpeg-8b/jmorecfg.h
    include/libjpeg-8b/jpeglib.h
    include/CameraEGLStreamConsumer.h
    include/libv4l2.h
    include/NvApplicationProfiler.h
    include/nvbuf_utils.h
    include/NvBuffer.h
    include/NvEglRenderer.h
    include/NvElement.h
    include/NvElementProfiler.h
    include/NvJpegDecoder.h
    include/NvJpegEncoder.h
    include/NvLogging.h
    include/nvosd.h
    include/NvUtils.h
    include/NvV4l2Element.h
    include/NvV4l2ElementPlane.h
    include/NvVideoConverter.h
    include/NvVideoDecoder.h
    include/NvVideoEncoder.h
    include/v4l2_nv_extensions.h
    samples/backend/v4l2_backend_csvparser.cpp
    samples/backend/v4l2_backend_main.cpp
    samples/backend/v4l2_backend_test.h
    samples/common/algorithm/cuda/NvAnalysis.h
    samples/common/algorithm/cuda/NvCudaProc.cpp
    samples/common/algorithm/cuda/NvCudaProc.h
    samples/common/algorithm/gie/gie_inference.cpp
    samples/common/algorithm/gie/gie_inference.h
    samples/common/classes/NvApplicationProfiler.cpp
    samples/common/classes/NvBuffer.cpp
    samples/common/classes/NvEglRenderer.cpp
    samples/common/classes/NvElement.cpp
    samples/common/classes/NvElementProfiler.cpp
    samples/common/classes/NvJpegDecoder.cpp
    samples/common/classes/NvJpegEncoder.cpp
    samples/common/classes/NvLogging.cpp
    samples/common/classes/NvUtils.cpp
    samples/common/classes/NvV4l2Element.cpp
    samples/common/classes/NvV4l2ElementPlane.cpp
    samples/common/classes/NvVideoConverter.cpp
    samples/common/classes/NvVideoDecoder.cpp
    samples/common/classes/NvVideoEncoder.cpp
)

# The real libtegravideo.so is excluded here because it should be built on a Tegra device.
add_executable(tegra_video EXCLUDE_FROM_ALL ${SOURCE_FILES})

add_library(tegra_video_stub ${TEGRA_VIDEO_STUB_SOURCE_FILES})

add_executable(net_dimensions_ut ${NET_DIMENSIONS_UT_SOURCE_FILES})

add_executable(tegra_video_stub_ut ${TEGRA_VIDEO_STUB_UT_SOURCE_FILES})

add_custom_target(run_tests
    DEPENDS
        tegra_video_stub_ut
        net_dimensions_ut
    COMMAND
        ./net_dimensions_ut && echo "" && ./tegra_video_stub_ut
)
