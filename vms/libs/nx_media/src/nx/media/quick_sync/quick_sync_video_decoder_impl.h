#pragma once

#include <vector>
#include <va/va.h>

#include "quick_sync_video_decoder.h"
#include <nx/utils/log/log.h>

#include <mfxvideo++.h>
#include "allocators/base_allocator.h"
#include "mfx_buffering.h"

namespace nx::media {

class QuickSyncVideoDecoderImpl
{
public:
    QuickSyncVideoDecoderImpl();
    ~QuickSyncVideoDecoderImpl();
    int decode(const QnConstCompressedVideoDataPtr& frame, nx::QVideoFramePtr* result = nullptr);

    void** getRenderingSurface() { return &m_renderingSurface; }

    static bool isCompatible(AVCodecID codec);

private:
    bool init(mfxBitstream& bitstream, AVCodecID codec);
    bool initSession();
    bool allocFrames();

private:
    struct OutputSurface
    {
        msdkFrameSurface* surface;
        mfxSyncPoint syncp;
    };

private:
    QuickSyncVideoDecoder::Config m_config;
    mfxVideoParam m_mfxDecParams;

    MFXVideoSession m_mfxSession;
    std::deque<OutputSurface> m_outputSurfaces;
    std::vector<uint8_t> m_bitstreamData;
    int64_t m_frameNumber = 0;

    // Temporary copy code from SDK
    bool allocFramesSample(mfxFrameAllocRequest& request);
    bool reallocCurrentSurface(mfxFrameSurface1& surface);
    bool syncOutputSurface(nx::QVideoFramePtr* result);
    CBuffering m_surfaces;
    std::shared_ptr<MFXFrameAllocator> m_allocator;
    mfxFrameAllocResponse m_response;
    msdkFrameSurface* m_pCurrentFreeSurface = nullptr; // Surface detached from free surfaces array.
    VADisplay m_display = nullptr;

    // Surface needed to render frame to opengl texture. Keep it here due to do not recreate it on
    // every frame.
    void* m_renderingSurface = nullptr;
};

} // namespace nx::media
