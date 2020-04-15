#pragma once

#include <vector>
#include <memory>

#include "quick_sync_video_decoder.h"

#include <mfx/mfxvideo++.h>
#include "allocators/base_allocator.h"
#include "utils.h"

namespace nx::media::quick_sync {

class QuickSyncVideoDecoderImpl : public std::enable_shared_from_this<QuickSyncVideoDecoderImpl>
{
public:
    QuickSyncVideoDecoderImpl();
    ~QuickSyncVideoDecoderImpl();
    int decode(const QnConstCompressedVideoDataPtr& frame, nx::QVideoFramePtr* result = nullptr);

    void lockSurface(const mfxFrameSurface1* surface);
    void releaseSurface(const mfxFrameSurface1* surface);

    Device& getDevice() { return m_device; }
    static bool isCompatible(AVCodecID codec);

private:
    struct SurfaceInfo
    {
        mfxFrameSurface1 surface;
        std::atomic<bool> isUsed = {false}; // signifies that frame is locked for rendering
    };

private:
    bool init(mfxBitstream& bitstream, AVCodecID codec);
    bool initSession();
    bool allocFrames();
    mfxFrameSurface1* getFreeSurface();

    bool buildQVideoFrame(mfxFrameSurface1* surface, nx::QVideoFramePtr* result);

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

    Device m_device;
};

} // namespace nx::media::quick_sync
