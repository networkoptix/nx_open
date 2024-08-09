// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/private/qabstractvideobuffer_p.h>

#include "quick_sync_surface.h"

namespace nx::media::quick_sync {

class QtVideoBuffer: public QAbstractVideoBuffer
{
public:
    QtVideoBuffer(QuickSyncSurface surfaceData):
        QAbstractVideoBuffer(QVideoFrame::NoHandle),
        m_surfaceData(surfaceData)
    {
        auto decoder = m_surfaceData.decoder.lock();
        if (!decoder)
            return;

        decoder->lockSurface(m_surfaceData.surface);
    }

    ~QtVideoBuffer()
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

    QVariant handle() const
    {
        return QVariant::fromValue(m_surfaceData);
    }

    virtual quint64 textureHandle(QRhi*, int /*plane*/) const override
    {
        return reinterpret_cast<quint64>(m_surfaceData.surface);
    }

private:
    QuickSyncSurface m_surfaceData;
};

} // namespace nx::media::quick_sync
