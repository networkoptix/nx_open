#pragma once

#include <memory>
#include <vector>
#include <mfx/mfxvideo++.h>

#include <QSize>

#include "allocators/base_allocator.h"

namespace nx::media::quick_sync {

class VppScaler
{
public:
    VppScaler(MFXVideoSession& session, std::shared_ptr<MFXFrameAllocator> allocator);
    ~VppScaler();
    bool scaleFrame(
        mfxFrameSurface1* inputSurface, mfxFrameSurface1** outSurface, const QSize& targetSize);
    bool init(const QSize& sourceSize, const QSize& targetSize);

private:
    void close();
    bool allocSurfaces(const mfxVideoParam& params);
    mfxVideoParam buildParam(const QSize& sourceSize, const QSize& targetSize);

private:
    MFXVideoSession& m_mfxSession; //< Maintained by decoder, should not be close.
    std::shared_ptr<MFXFrameAllocator> m_allocator; //< Maintained by decoder.
    std::unique_ptr<MFXVideoVPP> m_vpp;
    mfxFrameAllocResponse m_mfxResponseOut;
    std::vector<mfxFrameSurface1> m_outputSurfaces;
    mfxFrameAllocRequest m_VPPRequest[2]; // [0] - in, [1] - out
    QSize m_targetSize;
    QSize m_sourceSize;
};

} // namespace nx::media::quick_sync
