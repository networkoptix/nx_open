// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_video_decoder.h"
#include "NvDecoder.h"

#include <nx/media/nvidia/linux/renderer.h>
#include <nx/utils/log/log.h>
#include <media/filters/h264_mp4_to_annexb.h>
#include <nx/media/nvidia/nvidia_utils.h>
#include <nx/media/nvidia/nvidia_video_frame.h>

namespace {

inline cudaVideoCodec FFmpeg2NvCodecId(AVCodecID id) {
    switch (id) {
    case AV_CODEC_ID_MPEG1VIDEO : return cudaVideoCodec_MPEG1;
    case AV_CODEC_ID_MPEG2VIDEO : return cudaVideoCodec_MPEG2;
    case AV_CODEC_ID_MPEG4      : return cudaVideoCodec_MPEG4;
    case AV_CODEC_ID_WMV3       :
    case AV_CODEC_ID_VC1        : return cudaVideoCodec_VC1;
    case AV_CODEC_ID_H264       : return cudaVideoCodec_H264;
    case AV_CODEC_ID_HEVC       : return cudaVideoCodec_HEVC;
    case AV_CODEC_ID_VP8        : return cudaVideoCodec_VP8;
    case AV_CODEC_ID_VP9        : return cudaVideoCodec_VP9;
    case AV_CODEC_ID_MJPEG      : return cudaVideoCodec_JPEG;
    case AV_CODEC_ID_AV1        : return cudaVideoCodec_AV1;
    default                     : return cudaVideoCodec_NumCodecs;
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
            cuCtxDestroy(context);
    }
    CUcontext context = nullptr;
    std::unique_ptr<NvDecoder> decoder;
    std::unique_ptr<linux::Renderer> renderer;
    H2645Mp4ToAnnexB filterAnnexB;
    int framesReady = 0;
};

bool NvidiaVideoDecoder::isCompatible(
    const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int width, int height)
{
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
    CUresult status = cuInit(0);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to init nvidia decoder: %1", toString(status));
        return false;
    }

    CUdevice device = 0;
    status = cuDeviceGet(&device, 0/*gpu*/);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to get device: %1", toString(status));
        return false;
    }

    status = cuCtxCreate(&m_impl->context, CU_CTX_SCHED_BLOCKING_SYNC, device);
    if (status != CUDA_SUCCESS)
    {
        NX_WARNING(this, "Failed to create context: %1", toString(status));
        return false;
    }

    m_impl->decoder =
        std::make_unique<NvDecoder>(m_impl->context, true, FFmpeg2NvCodecId(frame->compressionType));

    return true;
}

int NvidiaVideoDecoder::decode(const QnConstCompressedVideoDataPtr& packet, nx::QVideoFramePtr* result)
{
    NX_DEBUG(this, "decode");
    if (!m_impl->decoder && !initialize(packet))
    {
        NX_ERROR(this, "Failed to initialize nvidia decoder");
        return -1;
    }

    auto packetAnnexB = m_impl->filterAnnexB.processVideoData(packet);

    m_impl->framesReady = m_impl->decoder->Decode(
        (const uint8_t*)packetAnnexB->data(), packetAnnexB->dataSize());
    NX_DEBUG(this, "framesReady: %1", m_impl->framesReady);
    //TODO crash here getFrame();
    return 1;
}

std::unique_ptr<NvidiaVideoFrame> NvidiaVideoDecoder::getFrame()
{
    if (!m_impl->framesReady)
        return nullptr;

    if (!m_impl->renderer)
    {
        m_impl->renderer = std::make_unique<linux::Renderer>();
        if (!m_impl->renderer->initialize(m_impl->decoder->GetWidth(), m_impl->decoder->GetHeight()))
        {
            NX_WARNING(this, "Failed to initialize renderer");
            return nullptr;
        }
    }

    auto frame = std::make_unique<NvidiaVideoFrame>();
    frame->decoder = weak_from_this();
    frame->height = m_impl->decoder->GetHeight();
    frame->width = m_impl->decoder->GetWidth();
    frame->frameData = m_impl->decoder->GetFrame(nullptr);
    frame->format = m_impl->decoder->GetOutputFormat();
    frame->bitDepth = m_impl->decoder->GetBitDepth();
    frame->matrix = m_impl->decoder->GetVideoFormatInfo().video_signal_description.matrix_coefficients;
    m_impl->renderer->convertToRgb(frame->frameData, (cudaVideoSurfaceFormat)frame->format, frame->bitDepth, frame->matrix);
    return frame;
}

//bool NvidiaVideoDecoderImpl::scaleFrame(
//      mfxFrameSurface1* inputSurface, mfxFrameSurface1** outSurface, const QSize& targetSize);

void NvidiaVideoDecoder::resetDecoder()
{

}

linux::Renderer& NvidiaVideoDecoder::getRenderer()
{
    return *m_impl->renderer;
}

} // namespace nx::media::nvidia