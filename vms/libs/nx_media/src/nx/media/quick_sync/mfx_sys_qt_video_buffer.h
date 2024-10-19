// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <sysmem_allocator.h>

#include <QtMultimedia/private/qhwvideobuffer_p.h>

#include <nx/utils/log/log.h>

namespace nx::media::quick_sync {

class MfxQtVideoBuffer: public QHwVideoBuffer
{
public:
    MfxQtVideoBuffer(
        mfxFrameSurface1* surface,
        const std::shared_ptr<MFXFrameAllocator>& allocator)
        :
        QHwVideoBuffer(QVideoFrame::NoHandle),
        m_allocator(allocator),
        m_surface(surface)
    {}

    virtual ~MfxQtVideoBuffer()
    {
    }

    virtual MapData map(QVideoFrame::MapMode /*mode*/) override
    {
        MapData data;
        m_allocator->LockFrame(m_surface->Data.MemId, &m_surface->Data);
        mfxU16 Height2 = (mfxU16) MSDK_ALIGN32(m_surface->Info.Height);
        data.dataSize[0] = m_surface->Data.PitchLow * Height2;
        data.dataSize[1] = m_surface->Data.PitchLow * Height2 / 2;
        data.dataSize[2] = m_surface->Data.PitchLow * Height2 / 2;
        data.dataSize[3] = 0;
        data.bytesPerLine[0] = m_surface->Data.PitchLow;
        data.bytesPerLine[1] = m_surface->Data.PitchLow;
        data.bytesPerLine[2] = m_surface->Data.PitchLow;
        data.bytesPerLine[3] = 0;
        data.data[0] = m_surface->Data.Y;
        data.data[1] = m_surface->Data.U;
        data.data[2] = m_surface->Data.V;
        data.data[3] = nullptr;
        data.planeCount = 3;
        return data;
    }

    virtual void unmap() override
    {
        m_allocator->UnlockFrame(m_surface->Data.MemId, &m_surface->Data);
    }

    virtual quint64 textureHandle(QRhi*, int /*plane*/) const override
    {
        return reinterpret_cast<quint64>(m_surface);
    }

    QVideoFrameFormat format() const override
    {
        if (!m_surface)
            return {};

        return QVideoFrameFormat(
            QSize(m_surface->Info.CropW, m_surface->Info.CropH),
            QVideoFrameFormat::Format_NV12);
    }

private:
    std::shared_ptr<MFXFrameAllocator> m_allocator;
    mfxFrameSurface1* m_surface = nullptr;
};

} // namespace nx::media::quick_sync
