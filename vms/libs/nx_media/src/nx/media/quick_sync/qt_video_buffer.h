// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtMultimedia/private/qhwvideobuffer_p.h>

#include "quick_sync_surface.h"
#include "quick_sync_video_decoder_impl.h"

namespace nx::media::quick_sync {

class QtVideoBuffer: public QHwVideoBuffer
{
public:
    QtVideoBuffer(QuickSyncSurface surfaceData):
        QHwVideoBuffer(QVideoFrame::NoHandle),
        m_surfaceData(surfaceData)
    {
        auto decoder = m_surfaceData.decoder.lock();
        if (!decoder)
            return;

        decoder->lockSurface(m_surfaceData.surface);
    }

    virtual ~QtVideoBuffer() override
    {
        auto decoder = m_surfaceData.decoder.lock();
        if (!decoder)
            return;

        decoder->releaseSurface(m_surfaceData.surface);
    }

    virtual MapData map(QVideoFrame::MapMode /*mode*/) override
    {
        return {};
    }

    virtual void unmap() override {}

    virtual quint64 textureHandle(QRhi*, int /*plane*/) const override
    {
        return reinterpret_cast<quint64>(m_surfaceData.surface);
    }

    QVideoFrameFormat format() const override
    {
        if (!m_surfaceData.surface)
            return {};

        return QVideoFrameFormat(
            QSize(m_surfaceData.surface->Info.CropW, m_surfaceData.surface->Info.CropH),
            QVideoFrameFormat::Format_NV12);
    }

private:
    QuickSyncSurface m_surfaceData;
};

} // namespace nx::media::quick_sync
