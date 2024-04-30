// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_video_decoder.h"
#include <NvCodec/NvDecoder.h>

#include <media/filters/h264_mp4_to_annexb.h>
#include <nx/media/nvidia/nvidia_driver_proxy.h>
#include <nx/media/nvidia/nvidia_utils.h>
#include <nx/media/nvidia/nvidia_video_frame.h>
#include <nx/media/nvidia/renderer.h>
#include <nx/utils/log/log.h>
#include <Utils/NvCodecUtils.h>

namespace {

inline cudaVideoCodec FFmpeg2NvCodecId(AVCodecID id)
{
    switch (id)
    {
        case AV_CODEC_ID_MPEG1VIDEO: return cudaVideoCodec_MPEG1;
        case AV_CODEC_ID_MPEG2VIDEO: return cudaVideoCodec_MPEG2;
        // Not supported case AV_CODEC_ID_MPEG4: return cudaVideoCodec_MPEG4;
        case AV_CODEC_ID_WMV3: return cudaVideoCodec_VC1;
        case AV_CODEC_ID_VC1: return cudaVideoCodec_VC1;
        case AV_CODEC_ID_H264: return cudaVideoCodec_H264;
        case AV_CODEC_ID_HEVC: return cudaVideoCodec_HEVC;
        case AV_CODEC_ID_VP8: return cudaVideoCodec_VP8;
        case AV_CODEC_ID_VP9: return cudaVideoCodec_VP9;
        case AV_CODEC_ID_MJPEG: return cudaVideoCodec_JPEG;
        case AV_CODEC_ID_AV1: return cudaVideoCodec_AV1;
        default: return cudaVideoCodec_NumCodecs;
    }
}

static constexpr int kCurrentCudaToolkitVersion = 11070;

}

namespace nx::media::nvidia {

struct NvidiaVideoDecoderImpl
{
    ~NvidiaVideoDecoderImpl()
    {
        NvidiaDriverApiProxy::instance().cuCtxPushCurrent(context);
        renderer.reset();
        decoder.reset();
        NvidiaDriverApiProxy::instance().cuCtxPopCurrent(nullptr);
        if (context)
            NvidiaDriverApiProxy::instance().cuCtxDestroy(context);
    }

    CUcontext context = nullptr;
    std::unique_ptr<NvDecoder> decoder;
    std::unique_ptr<Renderer> renderer;
    H2645Mp4ToAnnexB filterAnnexB;
};

bool NvidiaVideoDecoder::isAvailable()
{
    auto isAvalibaleImpl = []() {
        if (!NvidiaDriverApiProxy::instance().load())
        {
            NX_DEBUG(NX_SCOPE_TAG, "Failed to load nvidia driver");
            return false;
        }

        CUresult status = NvidiaDriverApiProxy::instance().cuInit(0);
        if (status != CUDA_SUCCESS)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Failed to init nvidia driver");
            return false;
        }

        int driverCudaVersion = 0;
        status = NvidiaDriverApiProxy::instance().cuDriverGetVersion(&driverCudaVersion);
        if (status != CUDA_SUCCESS)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Failed to get cuda version supported by driver");
            return false;
        }

        if (driverCudaVersion < kCurrentCudaToolkitVersion)
        {
            NX_DEBUG(NX_SCOPE_TAG,
                "Update nvidia driver, it supports cuda toolkit version: %1, but needed: %2",
                driverCudaVersion,
                kCurrentCudaToolkitVersion);
            return false;
        }

        return true;
    };
    static bool result = isAvalibaleImpl();
    return result;
}

bool NvidiaVideoDecoder::isCompatible(
    const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int /*width*/, int /*height*/)
{
    NvidiaVideoDecoder decoder(/*checkMode*/ true);
    if (!decoder.decode(frame))
        return false;

    // Push frame one more time to force parsing of video header to configure decoder settings.
    // NvDecoder::HandleVideoSequence for h264/h265 not called after passing one frame only.
    if (!decoder.decode(frame))
        return false;

    int kFreeMemoryLimit = 1024*1024*128;
    return freeGpuMemory() > kFreeMemoryLimit;
}

NvidiaVideoDecoder::NvidiaVideoDecoder(bool checkMode)
    : m_checkMode(checkMode)
{
    m_impl = std::make_unique<NvidiaVideoDecoderImpl>();
}

NvidiaVideoDecoder::~NvidiaVideoDecoder()
{
}

bool NvidiaVideoDecoder::initialize(const QnConstCompressedVideoDataPtr& frame)
{
    if (!NvidiaDriverApiProxy::instance().load())
        return false;

    CUresult status = NvidiaDriverApiProxy::instance().cuInit(0);
    if (status != CUDA_SUCCESS)
    {
        NX_DEBUG(this, "Failed to init nvidia decoder: %1", toString(status));
        return false;
    }

    CUdevice device = 0;
    status = NvidiaDriverApiProxy::instance().cuDeviceGet(&device, /*gpu*/0);
    if (status != CUDA_SUCCESS)
    {
        NX_DEBUG(this, "Failed to get device: %1", toString(status));
        return false;
    }

    status = NvidiaDriverApiProxy::instance().cuCtxCreate(&m_impl->context, CU_CTX_SCHED_BLOCKING_SYNC, device);
    if (status != CUDA_SUCCESS)
    {
        NX_DEBUG(this, "Failed to create context: %1", toString(status));
        return false;
    }
    try
    {
        m_impl->decoder = std::make_unique<NvDecoder>(
            m_impl->context,
            true,
            FFmpeg2NvCodecId(frame->compressionType),
            m_checkMode,
            /*bLowLatency*/ false);
    }
    catch (NVDECException& e)
    {
        NX_DEBUG(this, "Failed to create Nvidia decoder: %1", e.what());
        return false;
    }

    return true;
}

bool NvidiaVideoDecoder::decode(const QnConstCompressedVideoDataPtr& packet)
{
    if (!packet)
        return true; // Do nothing, flush decoder using getFrame call.

    if (!m_impl->decoder && !initialize(packet))
    {
        if (!m_checkMode)
            NX_ERROR(this, "Failed to initialize nvidia decoder");
        return false;
    }

    QnConstCompressedVideoDataPtr packetAnnexB;
    packetAnnexB = m_impl->filterAnnexB.processVideoData(packet);
    if (!packetAnnexB)
        packetAnnexB = packet;

    try
    {
        m_impl->decoder->Decode(
            (const uint8_t*)packetAnnexB->data(),
            packetAnnexB->dataSize(),
            0,
            packetAnnexB->timestamp);
    }
    catch (NVDECException& e)
    {
        if (!m_checkMode)
            NX_WARNING(this, "Failed to decode frame: %1", e.what());
        return false;
    }
    return true;
}

std::unique_ptr<NvidiaVideoFrame> NvidiaVideoDecoder::getFrame()
{
    try
    {
        auto& nvDecoder = m_impl->decoder;
        int64_t timestamp = 0;
        auto frameData = nvDecoder->GetFrame(&timestamp);
        if (!frameData)
            return nullptr;

        if (!m_impl->renderer)
            m_impl->renderer = std::make_unique<Renderer>();

        auto frame = std::make_unique<NvidiaVideoFrame>();
        frame->decoder = weak_from_this();
        frame->height = nvDecoder->GetHeight();
        frame->width = nvDecoder->GetWidth();
        frame->pitch = nvDecoder->GetDeviceFramePitch();
        frame->frameData = frameData;
        frame->timestamp = timestamp;
        frame->bitDepth = nvDecoder->GetBitDepth();
        frame->format = nvDecoder->GetOutputFormat();
        frame->matrix = nvDecoder->GetVideoFormatInfo().video_signal_description.matrix_coefficients;
        frame->bufferSize = nvDecoder->GetFrameSize();
        return frame;
    }
    catch (NVDECException& e)
    {
        NX_WARNING(this, "Failed to get frame: %1", e.what());
        return nullptr;
    }
}

void NvidiaVideoDecoder::releaseFrame(uint8_t* frame)
{
    m_impl->decoder->releaseFrame(frame);
}

Renderer& NvidiaVideoDecoder::getRenderer()
{
    return *m_impl->renderer;
}

void NvidiaVideoDecoder::pushContext()
{
    NvidiaDriverApiProxy::instance().cuCtxPushCurrent(m_impl->context);
}

void NvidiaVideoDecoder::popContext()
{
    NvidiaDriverApiProxy::instance().cuCtxPopCurrent(nullptr);
}

} // namespace nx::media::nvidia
