// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_driver_proxy.h"

#ifdef _WIN32
static constexpr std::string_view kNvcuvidLibraryName = "nvcuvid.dll";
static constexpr std::string_view kCudaLibraryName = "nvcuda.dll";
#else
static constexpr std::string_view kNvcuvidLibraryName = "libnvcuvid.so";
static constexpr std::string_view kCudaLibraryName = "libcuda.so";
#endif

namespace nx::media::nvidia {

bool NvidiaDriverDecoderProxy::load()
{
    if (cuvidMapVideoFrame)
        return true;

    if (!m_loader.load(kNvcuvidLibraryName.data()))
        return false;

    cuvidCreateVideoParser = (CuvidCreateVideoParserType) m_loader.getFunction("cuvidCreateVideoParser");
    if (!cuvidCreateVideoParser)
        return false;

    cuvidParseVideoData = (CuvidParseVideoDataType) m_loader.getFunction("cuvidParseVideoData");
    if (!cuvidParseVideoData)
        return false;

    cuvidDestroyVideoParser = (CuvidDestroyVideoParserType) m_loader.getFunction("cuvidDestroyVideoParser");
    if (!cuvidDestroyVideoParser)
        return false;

    cuvidCreateDecoder = (CuvidCreateDecoderType) m_loader.getFunction("cuvidCreateDecoder");
    if (!cuvidCreateDecoder)
        return false;

    cuvidGetDecoderCaps = (CuvidGetDecoderCapsType) m_loader.getFunction("cuvidGetDecoderCaps");
    if (!cuvidGetDecoderCaps)
        return false;

    cuvidDecodePicture = (CuvidDecodePictureType) m_loader.getFunction("cuvidDecodePicture");
    if (!cuvidDecodePicture)
        return false;

    cuvidGetDecodeStatus = (CuvidGetDecodeStatusType) m_loader.getFunction("cuvidGetDecodeStatus");
    if (!cuvidGetDecodeStatus)
        return false;

    cuvidDestroyDecoder = (CuvidDestroyDecoderType) m_loader.getFunction("cuvidDestroyDecoder");
    if (!cuvidDestroyDecoder)
        return false;

    cuvidCtxLockCreate = (CuvidCtxLockCreateType) m_loader.getFunction("cuvidCtxLockCreate");
    if (!cuvidCtxLockCreate)
        return false;

    cuvidCtxLockDestroy = (CuvidCtxLockDestroyType) m_loader.getFunction("cuvidCtxLockDestroy");
    if (!cuvidCtxLockDestroy)
        return false;

    cuvidUnmapVideoFrame = (CuvidUnmapVideoFrameType) m_loader.getFunction("cuvidUnmapVideoFrame64");
    if (!cuvidUnmapVideoFrame)
        return false;

    cuvidMapVideoFrame = (CuvidMapVideoFrameType) m_loader.getFunction("cuvidMapVideoFrame64");
    if (!cuvidMapVideoFrame)
        return false;

    return true;
}

bool NvidiaDriverApiProxy::load()
{
    if (cuGetErrorName)
        return true;

    if (!m_loader.load(kCudaLibraryName.data()))
        return false;

    cuInit = (CuInitType) m_loader.getFunction("cuInit");
    if (!cuInit)
        return false;

    cuDeviceGet = (CuDeviceGetType) m_loader.getFunction("cuDeviceGet");
    if (!cuDeviceGet)
        return false;

    cuCtxCreate = (CuCtxCreateType) m_loader.getFunction("cuCtxCreate_v2");
    if (!cuCtxCreate)
        return false;

    cuCtxDestroy = (CuCtxDestroyType) m_loader.getFunction("cuCtxDestroy_v2");
    if (!cuCtxDestroy)
        return false;

    cuCtxPopCurrent = (CuCtxPopCurrentType) m_loader.getFunction("cuCtxPopCurrent_v2");
    if (!cuCtxPopCurrent)
        return false;

    cuCtxPushCurrent = (CuCtxPushCurrentType) m_loader.getFunction("cuCtxPushCurrent_v2");
    if (!cuCtxPushCurrent)
        return false;

    cuMemcpy2DAsync = (CuMemcpy2DAsyncType) m_loader.getFunction("cuMemcpy2DAsync_v2");
    if (!cuMemcpy2DAsync)
        return false;

    cuMemFree = (CuMemFreeType) m_loader.getFunction("cuMemFree_v2");
    if (!cuMemFree)
        return false;

    cuMemAlloc = (CuMemAllocType) m_loader.getFunction("cuMemAlloc_v2");
    if (!cuMemAlloc)
        return false;

    cuMemAllocPitch = (CuMemAllocPitchType) m_loader.getFunction("cuMemAllocPitch_v2");
    if (!cuMemAllocPitch)
        return false;

    cuMemsetD8 = (CuMemsetD8Type) m_loader.getFunction("cuMemsetD8_v2");
    if (!cuMemsetD8)
        return false;

    cuGraphicsMapResources = (CuGraphicsMapResourcesType) m_loader.getFunction("cuGraphicsMapResources");
    if (!cuGraphicsMapResources)
        return false;

    cuGraphicsUnmapResources = (CuGraphicsUnmapResourcesType) m_loader.getFunction("cuGraphicsUnmapResources");
    if (!cuGraphicsUnmapResources)
        return false;

    cuGraphicsResourceGetMappedPointer = (CuGraphicsResourceGetMappedPointerType) m_loader.getFunction("cuGraphicsResourceGetMappedPointer_v2");
    if (!cuGraphicsResourceGetMappedPointer)
        return false;

    cuGraphicsGLRegisterBuffer = (CuGraphicsGLRegisterBufferType) m_loader.getFunction("cuGraphicsGLRegisterBuffer");
    if (!cuGraphicsGLRegisterBuffer)
        return false;

    cuGraphicsUnregisterResource = (CuGraphicsUnregisterResourceType) m_loader.getFunction("cuGraphicsUnregisterResource");
    if (!cuGraphicsUnregisterResource)
        return false;

    cuStreamSynchronize = (CuStreamSynchronizeType) m_loader.getFunction("cuStreamSynchronize");
    if (!cuStreamSynchronize)
        return false;

    cuGetErrorName = (CuGetErrorNameType) m_loader.getFunction("cuGetErrorName");
    if (!cuGetErrorName)
        return false;

    cuDriverGetVersion = (CuDriverGetVersionType) m_loader.getFunction("cuDriverGetVersion");
    if (!cuDriverGetVersion)
        return false;

    cuMemGetInfo = (CuMemGetInfo) m_loader.getFunction("cuMemGetInfo");
    if (!cuMemGetInfo)
        return false;

    cuCtxGetApiVersion = (CuCtxGetApiVersion) m_loader.getFunction("cuCtxGetApiVersion");
    if (!cuCtxGetApiVersion)
        return false;

    return true;
}

} // namespace nx::media::nvidia
