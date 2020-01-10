#pragma once

#include <QtMultimedia/QAbstractVideoBuffer>
#include <QtMultimedia/private/qabstractvideobuffer_p.h>

#include <nx/utils/log/log.h>

#include "mfx_buffering.h"

namespace nx::media {

class MfxQtVideoBufferPrivate: public QAbstractVideoBufferPrivate
{
public:
    std::shared_ptr<MFXFrameAllocator> allocator;
    msdkFrameSurface* surface;
    QAbstractVideoBuffer::MapMode mapMode;

public:
    MfxQtVideoBufferPrivate(msdkFrameSurface* surface, const std::shared_ptr<MFXFrameAllocator> allocator):
        allocator(allocator),
        surface(surface),
        mapMode(QAbstractVideoBuffer::NotMapped)
    {
    }

    virtual ~MfxQtVideoBufferPrivate()
    {
        msdk_atomic_dec16(&(surface->render_lock));
    }

    virtual int map(
        QAbstractVideoBuffer::MapMode /*mode*/,
        int* numBytes,
        int linesize[4],
        uchar* data[4]) override
    {
        allocator->LockFrame(surface->frame.Data.MemId, &surface->frame.Data);
        mfxU16 Height2 = (mfxU16)MSDK_ALIGN32(surface->frame.Info.Height);
        *numBytes = surface->frame.Data.PitchLow * Height2 * 2;
        linesize[0] = surface->frame.Data.PitchLow;
        linesize[1] = surface->frame.Data.PitchLow;
        linesize[2] = surface->frame.Data.PitchLow;
        linesize[3] = 0;
        data[0] = surface->frame.Data.Y;
        data[1] = surface->frame.Data.U;
        data[2] = surface->frame.Data.V;
        data[3] = nullptr;
        return 3;
    }
};

class MfxQtVideoBuffer: public QAbstractVideoBuffer
{

    Q_DECLARE_PRIVATE(MfxQtVideoBuffer)
public:
    MfxQtVideoBuffer(msdkFrameSurface* surface, const std::shared_ptr<MFXFrameAllocator> allocator):
        QAbstractVideoBuffer(*(new MfxQtVideoBufferPrivate(surface, allocator)), NoHandle)
    {}

    virtual MapMode mapMode() const override
    {
        return d_func()->mapMode;
    }

    virtual uchar* map(MapMode /*mode*/, int *numBytes, int *bytesPerLine) override
    {
        Q_D(MfxQtVideoBuffer);

        d->allocator->LockFrame(d->surface->frame.Data.MemId, &d->surface->frame.Data);
        *bytesPerLine = d->surface->frame.Data.PitchLow;
        *numBytes = GetSurfaceSize(
            d->surface->frame.Info.FourCC,
            MSDK_ALIGN32(d->surface->frame.Info.Width),
            MSDK_ALIGN32(d->surface->frame.Info.Height));
        return d->surface->frame.Data.Y;
    }

    virtual void unmap() override
    {
        Q_D(MfxQtVideoBuffer);
        d->allocator->UnlockFrame(d->surface->frame.Data.MemId, &d->surface->frame.Data);
    }
};

} // namespace nx::media
