#pragma once

#include <QtMultimedia/QAbstractVideoBuffer>

#include "va_surface_info.h"

namespace nx::media::quick_sync {

class VaQtVideoBuffer: public QAbstractVideoBuffer
{
public:
    VaQtVideoBuffer(VaSurfaceInfo surfaceData):
        QAbstractVideoBuffer(QAbstractVideoBuffer::HandleType(kHandleTypeQsvSurface)),
        m_surfaceData(surfaceData)
    {
        auto decoder = m_surfaceData.decoder.lock();
        if (!decoder)
            return;

        decoder->lockSurface(m_surfaceData.surface);
    }

    ~VaQtVideoBuffer()
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
    VaSurfaceInfo m_surfaceData;
};

} // namespace nx::media::quick_sync


