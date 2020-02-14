#pragma once

#include <QtMultimedia/QAbstractVideoBuffer>

#include <nx/utils/log/log.h>

#include "mfx_buffering.h"
#include "va_surface_info.h"

namespace nx::media {

class MfxDrmQtVideoBuffer: public QAbstractVideoBuffer
{
public:
    MfxDrmQtVideoBuffer(msdkFrameSurface* surface, VaSurfaceInfo surfaceData):
        QAbstractVideoBuffer(QAbstractVideoBuffer::HandleType(kHandleTypeVaSurface)),
        m_surfaceData(surfaceData),
        m_surface(surface)
    {}

    virtual ~MfxDrmQtVideoBuffer()
    {
        msdk_atomic_dec16(&(m_surface->render_lock));
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
    msdkFrameSurface* m_surface;
};

} // namespace nx::media


