#pragma once

#include <QtMultimedia/QAbstractVideoBuffer>

#include <nx/utils/log/log.h>

#include "va_surface_info.h"

namespace nx::media {

class MfxDrmQtVideoBuffer: public QAbstractVideoBuffer
{
public:
    MfxDrmQtVideoBuffer(std::atomic<bool>* isRendered, VaSurfaceInfo surfaceData):
        QAbstractVideoBuffer(QAbstractVideoBuffer::HandleType(kHandleTypeVaSurface)),
        m_surfaceData(surfaceData),
        m_isRendered(isRendered)
    {}

    virtual ~MfxDrmQtVideoBuffer()
    {
        *m_isRendered = true;
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
    std::atomic<bool>* m_isRendered;
};

} // namespace nx::media


