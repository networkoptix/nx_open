// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nvcuvid.h>

#include <QtGui/QOpenGLFunctions>

#include <nx/media/nvidia/linux/library_loader.h>

namespace nx::media::nvidia {

class NvidiaDriverDecoderProxy
{
public:
    bool load();
    static NvidiaDriverDecoderProxy& instance()
    {
        static NvidiaDriverDecoderProxy proxy;
        return proxy;
    }

public:
    using cuvidCreateDecoderType = CUresult (*)(CUvideodecoder*, CUVIDDECODECREATEINFO*);
    using cuvidReconfigureDecoderType = CUresult (*)(CUvideodecoder, CUVIDRECONFIGUREDECODERINFO*);
    using cuvidDestroyDecoderType = CUresult (*)(CUvideodecoder);

    using cuvidCreateVideoParserType = CUresult (*)(CUvideoparser*, CUVIDPARSERPARAMS*);
    using cuvidParseVideoDataType = CUresult (*)(CUvideoparser, CUVIDSOURCEDATAPACKET*);
    using cuvidDestroyVideoParserType = CUresult (*)(CUvideoparser);

    using cuvidCtxLockCreateType = CUresult (*)(CUvideoctxlock*, CUcontext);
    using cuvidCtxLockDestroyType = CUresult (*)(CUvideoctxlock);

    using cuvidGetDecoderCapsType = CUresult (*)(CUVIDDECODECAPS*);
    using cuvidDecodePictureType = CUresult (*)(CUvideodecoder, CUVIDPICPARAMS*);
    using cuvidGetDecodeStatusType = CUresult (*)(CUvideodecoder, int, CUVIDGETDECODESTATUS*);

    using cuvidMapVideoFrameType = CUresult (*)(CUvideodecoder, int, unsigned long long*, unsigned int*, CUVIDPROCPARAMS*);
    using cuvidUnmapVideoFrameType = CUresult (*)(CUvideodecoder, unsigned long long);

    cuvidCreateVideoParserType cuvidCreateVideoParser = nullptr;
    cuvidParseVideoDataType cuvidParseVideoData = nullptr;
    cuvidDestroyVideoParserType cuvidDestroyVideoParser = nullptr;

    cuvidCreateDecoderType cuvidCreateDecoder = nullptr;
    cuvidReconfigureDecoderType cuvidReconfigureDecoder = nullptr;
    cuvidGetDecoderCapsType cuvidGetDecoderCaps = nullptr;
    cuvidDecodePictureType cuvidDecodePicture = nullptr;
    cuvidGetDecodeStatusType cuvidGetDecodeStatus = nullptr;
    cuvidDestroyDecoderType cuvidDestroyDecoder = nullptr;

    cuvidUnmapVideoFrameType cuvidUnmapVideoFrame = nullptr;
    cuvidMapVideoFrameType cuvidMapVideoFrame = nullptr;

    cuvidCtxLockCreateType cuvidCtxLockCreate = nullptr;
    cuvidCtxLockDestroyType cuvidCtxLockDestroy = nullptr;

private:
    linux::LibraryLoader m_loader;
};

class NvidiaDriverApiProxy
{
public:
    bool load();
    static NvidiaDriverApiProxy& instance()
    {
        static NvidiaDriverApiProxy proxy;
        return proxy;
    }

public:
    using cuInitType = CUresult (*)(unsigned int);
    using cuCtxCreateType = CUresult (*)(CUcontext*, unsigned int, CUdevice);
    using cuCtxDestroyType = CUresult (*)(CUcontext);
    using cuDeviceGetType = CUresult (*)(CUdevice*, int);
    using cuCtxPopCurrentType = CUresult (*)(CUcontext*);
    using cuCtxPushCurrentType = CUresult (*)(CUcontext);
    using cuMemcpy2DAsyncType = CUresult (*)(const CUDA_MEMCPY2D*, CUstream);
    using cuStreamSynchronizeType = CUresult (*)(CUstream);
    using cuGetErrorNameType = CUresult (*)(CUresult, const char**);
    using cuMemFreeType = CUresult (*)(CUdeviceptr);
    using cuMemAllocType = CUresult (*)(CUdeviceptr*, size_t);
    using cuMemAllocPitchType = CUresult (*)(CUdeviceptr*, size_t*, size_t, size_t, unsigned int);
    using cuGraphicsUnregisterResourceType = CUresult (*)(CUgraphicsResource);
    using cuGraphicsGLRegisterBufferType = CUresult (*)(CUgraphicsResource*, GLuint, unsigned int);
    using cuGraphicsMapResourcesType = CUresult (*)(unsigned int, CUgraphicsResource*, CUstream);
    using cuGraphicsUnmapResourcesType = CUresult (*)(unsigned int, CUgraphicsResource*, CUstream);
    using cuGraphicsResourceGetMappedPointerType = CUresult (*)(CUdeviceptr*, size_t*, CUgraphicsResource);
    using cuMemsetD8Type = CUresult (*)(CUdeviceptr, unsigned char, size_t);

    cuInitType cuInit = nullptr;
    cuCtxCreateType cuCtxCreate = nullptr;
    cuCtxDestroyType cuCtxDestroy = nullptr;
    cuDeviceGetType cuDeviceGet = nullptr;
    cuMemFreeType cuMemFree = nullptr;
    cuMemAllocType cuMemAlloc = nullptr;
    cuMemAllocPitchType cuMemAllocPitch = nullptr;
    cuMemsetD8Type cuMemsetD8 = nullptr;
    cuGraphicsMapResourcesType cuGraphicsMapResources = nullptr;
    cuGraphicsResourceGetMappedPointerType cuGraphicsResourceGetMappedPointer = nullptr;
    cuGraphicsUnmapResourcesType cuGraphicsUnmapResources = nullptr;
    cuGraphicsGLRegisterBufferType cuGraphicsGLRegisterBuffer = nullptr;
    cuGraphicsUnregisterResourceType cuGraphicsUnregisterResource = nullptr;
    cuCtxPopCurrentType cuCtxPopCurrent = nullptr;
    cuCtxPushCurrentType cuCtxPushCurrent = nullptr;
    cuMemcpy2DAsyncType cuMemcpy2DAsync = nullptr;
    cuStreamSynchronizeType cuStreamSynchronize = nullptr;
    cuGetErrorNameType cuGetErrorName = nullptr;

private:
    linux::LibraryLoader m_loader;
};

} // namespace nx::media::nvidia
