// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#include "nvidia_driver_proxy.h"

namespace nx::media::nvidia {

bool NvidiaDriverDecoderProxy::load()
{
    if (cuvidMapVideoFrame)
        return true;

    if (!m_loader.load("libnvcuvid.so"))
        return false;

    cuvidCreateVideoParser = (cuvidCreateVideoParserType)m_loader.getFunction("cuvidCreateVideoParser");
    if (!cuvidCreateVideoParser)
        return false;

    cuvidParseVideoData = (cuvidParseVideoDataType)m_loader.getFunction("cuvidParseVideoData");
    if (!cuvidParseVideoData)
        return false;

    cuvidDestroyVideoParser = (cuvidDestroyVideoParserType)m_loader.getFunction("cuvidDestroyVideoParser");
    if (!cuvidDestroyVideoParser)
        return false;

    cuvidCreateDecoder = (cuvidCreateDecoderType)m_loader.getFunction("cuvidCreateDecoder");
    if (!cuvidCreateDecoder)
        return false;

    cuvidGetDecoderCaps = (cuvidGetDecoderCapsType)m_loader.getFunction("cuvidGetDecoderCaps");
    if (!cuvidGetDecoderCaps)
        return false;

    cuvidDecodePicture = (cuvidDecodePictureType)m_loader.getFunction("cuvidDecodePicture");
    if (!cuvidDecodePicture)
        return false;

    cuvidGetDecodeStatus = (cuvidGetDecodeStatusType)m_loader.getFunction("cuvidGetDecodeStatus");
    if (!cuvidGetDecodeStatus)
        return false;

    cuvidDestroyDecoder = (cuvidDestroyDecoderType)m_loader.getFunction("cuvidDestroyDecoder");
    if (!cuvidDestroyDecoder)
        return false;

    cuvidCtxLockCreate = (cuvidCtxLockCreateType)m_loader.getFunction("cuvidCtxLockCreate");
    if (!cuvidCtxLockCreate)
        return false;

    cuvidCtxLockDestroy = (cuvidCtxLockDestroyType)m_loader.getFunction("cuvidCtxLockDestroy");
    if (!cuvidCtxLockDestroy)
        return false;

    cuvidUnmapVideoFrame = (cuvidUnmapVideoFrameType)m_loader.getFunction("cuvidUnmapVideoFrame64");
    if (!cuvidUnmapVideoFrame)
        return false;

    cuvidMapVideoFrame = (cuvidMapVideoFrameType)m_loader.getFunction("cuvidMapVideoFrame64");
    if (!cuvidMapVideoFrame)
        return false;

    return true;
}

bool NvidiaDriverApiProxy::load()
{
    if (cuGetErrorName)
        return true;

    if (!m_loader.load("libcuda.so"))
        return false;

    cuInit = (cuInitType)m_loader.getFunction("cuInit");
    if (!cuInit)
        return false;

    cuDeviceGet = (cuDeviceGetType)m_loader.getFunction("cuDeviceGet");
    if (!cuDeviceGet)
        return false;

    cuCtxCreate = (cuCtxCreateType)m_loader.getFunction("cuCtxCreate_v2");
    if (!cuCtxCreate)
        return false;

    cuCtxDestroy = (cuCtxDestroyType)m_loader.getFunction("cuCtxDestroy_v2");
    if (!cuCtxDestroy)
        return false;

    cuCtxPopCurrent = (cuCtxPopCurrentType)m_loader.getFunction("cuCtxPopCurrent_v2");
    if (!cuCtxPopCurrent)
        return false;

    cuCtxPushCurrent = (cuCtxPushCurrentType)m_loader.getFunction("cuCtxPushCurrent_v2");
    if (!cuCtxPushCurrent)
        return false;

    cuMemcpy2DAsync = (cuMemcpy2DAsyncType)m_loader.getFunction("cuMemcpy2DAsync_v2");
    if (!cuMemcpy2DAsync)
        return false;

    cuMemFree = (cuMemFreeType)m_loader.getFunction("cuMemFree_v2");
    if (!cuMemFree)
        return false;

    cuMemAlloc = (cuMemAllocType)m_loader.getFunction("cuMemAlloc_v2");
    if (!cuMemAlloc)
        return false;

    cuMemAllocPitch = (cuMemAllocPitchType)m_loader.getFunction("cuMemAllocPitch_v2");
    if (!cuMemAllocPitch)
        return false;

    cuMemsetD8 = (cuMemsetD8Type)m_loader.getFunction("cuMemsetD8_v2");
    if (!cuMemsetD8)
        return false;

    cuGraphicsMapResources = (cuGraphicsMapResourcesType)m_loader.getFunction("cuGraphicsMapResources");
    if (!cuGraphicsMapResources)
        return false;

    cuGraphicsUnmapResources = (cuGraphicsUnmapResourcesType)m_loader.getFunction("cuGraphicsUnmapResources");
    if (!cuGraphicsUnmapResources)
        return false;

    cuGraphicsResourceGetMappedPointer = (cuGraphicsResourceGetMappedPointerType)m_loader.getFunction("cuGraphicsResourceGetMappedPointer_v2");
    if (!cuGraphicsResourceGetMappedPointer)
        return false;

    cuGraphicsGLRegisterBuffer = (cuGraphicsGLRegisterBufferType)m_loader.getFunction("cuGraphicsGLRegisterBuffer");
    if (!cuGraphicsGLRegisterBuffer)
        return false;

    cuGraphicsUnregisterResource = (cuGraphicsUnregisterResourceType)m_loader.getFunction("cuGraphicsUnregisterResource");
    if (!cuGraphicsUnregisterResource)
        return false;

    cuStreamSynchronize = (cuStreamSynchronizeType)m_loader.getFunction("cuStreamSynchronize");
    if (!cuStreamSynchronize)
        return false;

    cuGetErrorName = (cuGetErrorNameType)m_loader.getFunction("cuGetErrorName");
    if (!cuGetErrorName)
        return false;

    return true;
}

} // namespace nx::media::nvidia
