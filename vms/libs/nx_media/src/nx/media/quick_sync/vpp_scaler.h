// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <base_allocator.h>

#include <QtCore/QSize>

#include <vpl/mfx.h>
#include <vpl/mfxvideo++.h>

namespace nx::media::quick_sync {

class NX_MEDIA_API VppScaler
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
