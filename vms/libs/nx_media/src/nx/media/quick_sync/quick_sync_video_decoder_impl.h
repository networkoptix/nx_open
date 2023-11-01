// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <memory>
#include <vector>

#include <base_allocator.h>

#include <mfx/mfxvideo++.h>

#include "quick_sync_video_decoder.h"
#include "utils.h"
#include "vpp_scaler.h"

namespace nx::media::quick_sync {

class NX_MEDIA_API QuickSyncVideoDecoderImpl : public std::enable_shared_from_this<QuickSyncVideoDecoderImpl>
{
public:
    QuickSyncVideoDecoderImpl();
    ~QuickSyncVideoDecoderImpl();
    int decode(const QnConstCompressedVideoDataPtr& frame, nx::media::VideoFramePtr* result = nullptr);

    void lockSurface(const mfxFrameSurface1* surface);
    void releaseSurface(const mfxFrameSurface1* surface);

    DeviceContext& getDevice() { return m_device; }
    static bool isCompatible(const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int width, int height);
    static bool isAvailable();

    bool scaleFrame(
        mfxFrameSurface1* inputSurface, mfxFrameSurface1** outSurface, const QSize& targetSize);

    void resetDecoder();

private:
    struct SurfaceInfo
    {
        mfxFrameSurface1 surface;
        std::atomic<bool> isUsed = {false}; // signifies that frame is locked for rendering
    };

private:
    bool init(mfxBitstream& bitstream, const QnConstCompressedVideoDataPtr& frame);
    bool initSession();
    bool initDevice(int width, int height);
    bool allocFrames();
    mfxFrameSurface1* getFreeSurface();

    bool buildQVideoFrame(mfxFrameSurface1* surface, nx::media::VideoFramePtr* result);
    std::unique_ptr<mfxBitstream> updateBitStream(const QnConstCompressedVideoDataPtr& frame);
    void clearData();

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

    DeviceContext m_device;
    std::deque<int64_t> m_dtsQueue;

    std::unique_ptr<VppScaler> m_scaler;
    bool m_decoderInitialized = false;
};

} // namespace nx::media::quick_sync
