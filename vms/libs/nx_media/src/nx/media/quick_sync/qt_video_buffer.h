#pragma once

#include <QtMultimedia/QAbstractVideoBuffer>

#include "quick_sync_surface.h"

namespace nx::media::quick_sync {

class QtVideoBuffer: public QAbstractVideoBuffer
{
public:
    QtVideoBuffer(QuickSyncSurface surfaceData):
        QAbstractVideoBuffer(QAbstractVideoBuffer::HandleType(kHandleTypeQsvSurface)),
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

    virtual MapMode mapMode() const override
    {
        return NotMapped;
    }

    virtual uchar* map(MapMode /*mode*/, int */*numBytes*/, int */*bytesPerLine*/) override
    {
        return nullptr;
    }

    virtual void unmap() override {}

    QVariant handle() const
    {
        return QVariant::fromValue(m_surfaceData);
    }

private:
    QuickSyncSurface m_surfaceData;
};

} // namespace nx::media::quick_sync


