// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_video_decoder.h"
#include <NvCodec/NvDecoder.h>

#include <nx/media/nvidia/renderer.h>
#include <nx/utils/log/log.h>
#include <media/filters/h264_mp4_to_annexb.h>
#include <nx/media/nvidia/nvidia_utils.h>
#include <nx/media/nvidia/nvidia_video_frame.h>

#include <nx/media/nvidia/nvidia_driver_proxy.h>

namespace {

inline cudaVideoCodec FFmpeg2NvCodecId(AVCodecID id)
{
    switch (id)
    {
        case AV_CODEC_ID_MPEG1VIDEO: return cudaVideoCodec_MPEG1;
        case AV_CODEC_ID_MPEG2VIDEO: return cudaVideoCodec_MPEG2;
        case AV_CODEC_ID_MPEG4: return cudaVideoCodec_MPEG4;
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

}

namespace nx::media::nvidia {

struct NvidiaVideoDecoderImpl
{
    ~NvidiaVideoDecoderImpl()
    {
        decoder.reset();
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
    if (!NvidiaDriverApiProxy::instance().load())
        return false;

    CUresult status = NvidiaDriverApiProxy::instance().cuInit(0);
    if (status != CUDA_SUCCESS)
        return false;

    return true;
}

bool NvidiaVideoDecoder::isCompatible(
    const QnConstCompressedVideoDataPtr& /*frame*/, AVCodecID /*codec*/, int /*width*/, int /*height*/)
{
    if (!NvidiaDriverApiProxy::instance().load())
        return false;

    CUresult status = NvidiaDriverApiProxy::instance().cuInit(0);
    if (status != CUDA_SUCCESS)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to init nvidia decoder: %1", toString(status));
        return false;
    }

    CUdevice device = 0;
    status = NvidiaDriverApiProxy::instance().cuDeviceGet(&device, /*gpu*/0);
    if (status != CUDA_SUCCESS)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to get device: %1", toString(status));
        return false;
    }
    return true;
}

NvidiaVideoDecoder::NvidiaVideoDecoder()
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
        NX_WARNING(this, "Failed to init nvidia decoder: %1", toString(status));
        return false;
    }

    CUdevice device = 0;
    status = NvidiaDriverApiProxy::instance().cuDeviceGet(&device, /*gpu*/0);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to get device: %1", toString(status));
        return false;
    }

    status = NvidiaDriverApiProxy::instance().cuCtxCreate(&m_impl->context, CU_CTX_SCHED_BLOCKING_SYNC, device);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to create context: %1", toString(status));
        return false;
    }
    try
    {
        m_impl->decoder = std::make_unique<NvDecoder>(
            m_impl->context,
            true,
            FFmpeg2NvCodecId(frame->compressionType),
            /*bLowLatency*/ false);
    }
    catch (NVDECException& e)
    {
        NX_WARNING(this, "Failed to create Nvidia decoder: %1", e.what());
        return false;
    }

    return true;
}

int NvidiaVideoDecoder::decode(const QnConstCompressedVideoDataPtr& packet)
{
    if (!m_impl->decoder && !initialize(packet))
    {
        NX_ERROR(this, "Failed to initialize nvidia decoder");
        return -1;
    }

    auto packetAnnexB = m_impl->filterAnnexB.processVideoData(packet);

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
        NX_WARNING(this, "Failed to decode frame: %1", e.what());
        return false;
    }
    return 1;
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
