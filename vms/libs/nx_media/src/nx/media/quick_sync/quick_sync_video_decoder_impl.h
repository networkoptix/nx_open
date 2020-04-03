#pragma once

#include <vector>
#include <va/va.h>

#include "quick_sync_video_decoder.h"
#include <nx/utils/log/log.h>

#include <mfx/mfxvideo++.h>
#include "allocators/base_allocator.h"

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
    struct SurfaceInfo
    {
        mfxFrameSurface1 surface;
        std::atomic<bool> isRendered = {true}; // signifies that frame is locked for rendering
    };

private:
    bool init(mfxBitstream& bitstream, AVCodecID codec);
    bool initSession();
    bool allocFrames();
    SurfaceInfo* getFreeSurface();
    void buildQVideoFrame(SurfaceInfo& surface, nx::QVideoFramePtr* result) const;

private:
    QuickSyncVideoDecoder::Config m_config;
    mfxVideoParam m_mfxDecParams;

    MFXVideoSession m_mfxSession;
    std::vector<uint8_t> m_bitstreamData;
    std::vector<SurfaceInfo> m_surfaces;
    int64_t m_frameNumber = 0;

    // Temporary copy code from SDK
    bool allocSurfaces(mfxFrameAllocRequest& request);
    std::shared_ptr<MFXFrameAllocator> m_allocator;
    mfxFrameAllocResponse m_response;
    VADisplay m_display = nullptr;

    // Surface needed to render frame to opengl texture. Keep it here due to do not recreate it on
    // every frame.
    void* m_renderingSurface = nullptr;
};

} // namespace nx::media
