// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nvcuvid.h>

#include <QtGui/QOpenGLFunctions>

#include <nx/media/nvidia/library_loader.h>

namespace nx::media::nvidia {

class NX_MEDIA_API NvidiaDriverDecoderProxy
{
public:
    bool load();

    static NvidiaDriverDecoderProxy& instance()
    {
        static NvidiaDriverDecoderProxy proxy;
        return proxy;
    }

public:
    using CuvidCreateDecoderType = CUresult (*)(CUvideodecoder*, CUVIDDECODECREATEINFO*);
    using CuvidReconfigureDecoderType = CUresult (*)(CUvideodecoder, CUVIDRECONFIGUREDECODERINFO*);
    using CuvidDestroyDecoderType = CUresult (*)(CUvideodecoder);

    using CuvidCreateVideoParserType = CUresult (*)(CUvideoparser*, CUVIDPARSERPARAMS*);
    using CuvidParseVideoDataType = CUresult (*)(CUvideoparser, CUVIDSOURCEDATAPACKET*);
    using CuvidDestroyVideoParserType = CUresult (*)(CUvideoparser);

    using CuvidCtxLockCreateType = CUresult (*)(CUvideoctxlock*, CUcontext);
    using CuvidCtxLockDestroyType = CUresult (*)(CUvideoctxlock);

    using CuvidGetDecoderCapsType = CUresult (*)(CUVIDDECODECAPS*);
    using CuvidDecodePictureType = CUresult (*)(CUvideodecoder, CUVIDPICPARAMS*);
    using CuvidGetDecodeStatusType = CUresult (*)(CUvideodecoder, int, CUVIDGETDECODESTATUS*);

    using CuvidMapVideoFrameType = CUresult (*)(CUvideodecoder, int, unsigned long long*, unsigned int*, CUVIDPROCPARAMS*);
    using CuvidUnmapVideoFrameType = CUresult (*)(CUvideodecoder, unsigned long long);

    CuvidCreateVideoParserType cuvidCreateVideoParser = nullptr;
    CuvidParseVideoDataType cuvidParseVideoData = nullptr;
    CuvidDestroyVideoParserType cuvidDestroyVideoParser = nullptr;

    CuvidCreateDecoderType cuvidCreateDecoder = nullptr;
    CuvidReconfigureDecoderType cuvidReconfigureDecoder = nullptr;
    CuvidGetDecoderCapsType cuvidGetDecoderCaps = nullptr;
    CuvidDecodePictureType cuvidDecodePicture = nullptr;
    CuvidGetDecodeStatusType cuvidGetDecodeStatus = nullptr;
    CuvidDestroyDecoderType cuvidDestroyDecoder = nullptr;

    CuvidUnmapVideoFrameType cuvidUnmapVideoFrame = nullptr;
    CuvidMapVideoFrameType cuvidMapVideoFrame = nullptr;

    CuvidCtxLockCreateType cuvidCtxLockCreate = nullptr;
    CuvidCtxLockDestroyType cuvidCtxLockDestroy = nullptr;

private:
    LibraryLoader m_loader;
};

class NX_MEDIA_API NvidiaDriverApiProxy
{
public:
    bool load();
    static NvidiaDriverApiProxy& instance()
    {
        static NvidiaDriverApiProxy proxy;
        return proxy;
    }

public:
    using CuInitType = CUresult (*)(unsigned int);
    using CuCtxCreateType = CUresult (*)(CUcontext*, unsigned int, CUdevice);
    using CuCtxDestroyType = CUresult (*)(CUcontext);
    using CuDeviceGetType = CUresult (*)(CUdevice*, int);
    using CuCtxPopCurrentType = CUresult (*)(CUcontext*);
    using CuCtxPushCurrentType = CUresult (*)(CUcontext);
    using CuMemcpy2DAsyncType = CUresult (*)(const CUDA_MEMCPY2D*, CUstream);
    using CuStreamSynchronizeType = CUresult (*)(CUstream);
    using CuGetErrorNameType = CUresult (*)(CUresult, const char**);
    using CuMemFreeType = CUresult (*)(CUdeviceptr);
    using CuMemAllocType = CUresult (*)(CUdeviceptr*, size_t);
    using CuMemAllocPitchType = CUresult (*)(CUdeviceptr*, size_t*, size_t, size_t, unsigned int);
    using CuGraphicsUnregisterResourceType = CUresult (*)(CUgraphicsResource);
    using CuGraphicsGLRegisterBufferType = CUresult (*)(CUgraphicsResource*, GLuint, unsigned int);
    using CuGraphicsMapResourcesType = CUresult (*)(unsigned int, CUgraphicsResource*, CUstream);
    using CuGraphicsUnmapResourcesType = CUresult (*)(unsigned int, CUgraphicsResource*, CUstream);
    using CuGraphicsResourceGetMappedPointerType = CUresult (*)(CUdeviceptr*, size_t*, CUgraphicsResource);
    using CuMemsetD8Type = CUresult (*)(CUdeviceptr, unsigned char, size_t);
    using CuDriverGetVersionType = CUresult (*)(int*);
    using CuMemGetInfo = CUresult (*)(size_t*, size_t*);
    using CuCtxGetApiVersion = CUresult (*)(CUcontext, unsigned int*);


    CuInitType cuInit = nullptr;
    CuCtxCreateType cuCtxCreate = nullptr;
    CuCtxDestroyType cuCtxDestroy = nullptr;
    CuDeviceGetType cuDeviceGet = nullptr;
    CuMemFreeType cuMemFree = nullptr;
    CuMemAllocType cuMemAlloc = nullptr;
    CuMemAllocPitchType cuMemAllocPitch = nullptr;
    CuMemsetD8Type cuMemsetD8 = nullptr;
    CuGraphicsMapResourcesType cuGraphicsMapResources = nullptr;
    CuGraphicsResourceGetMappedPointerType cuGraphicsResourceGetMappedPointer = nullptr;
    CuGraphicsUnmapResourcesType cuGraphicsUnmapResources = nullptr;
    CuGraphicsGLRegisterBufferType cuGraphicsGLRegisterBuffer = nullptr;
    CuGraphicsUnregisterResourceType cuGraphicsUnregisterResource = nullptr;
    CuCtxPopCurrentType cuCtxPopCurrent = nullptr;
    CuCtxPushCurrentType cuCtxPushCurrent = nullptr;
    CuMemcpy2DAsyncType cuMemcpy2DAsync = nullptr;
    CuStreamSynchronizeType cuStreamSynchronize = nullptr;
    CuGetErrorNameType cuGetErrorName = nullptr;
    CuDriverGetVersionType cuDriverGetVersion = nullptr;
    CuMemGetInfo cuMemGetInfo = nullptr;
    CuCtxGetApiVersion cuCtxGetApiVersion = nullptr;

private:
    LibraryLoader m_loader;
};

} // namespace nx::media::nvidia
