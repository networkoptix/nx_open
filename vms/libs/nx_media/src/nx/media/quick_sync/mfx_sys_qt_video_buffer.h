// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtMultimedia/QAbstractVideoBuffer>
#include <QtMultimedia/private/qabstractvideobuffer_p.h>

#include <nx/utils/log/log.h>

namespace nx::media::quick_sync {

class MfxQtVideoBufferPrivate: public QAbstractVideoBufferPrivate
{
public:
    MfxQtVideoBufferPrivate(
        mfxFrameSurface1* surface, const std::shared_ptr<MFXFrameAllocator>& allocator):
        allocator(allocator),
        surface(surface),
        mapMode(QAbstractVideoBuffer::NotMapped)
    {
    }

    virtual int map(
        QAbstractVideoBuffer::MapMode /*mode*/,
        int* numBytes,
        int linesize[4],
        uchar* data[4]) override
    {
        allocator->LockFrame(surface->Data.MemId, &surface->Data);
        mfxU16 Height2 = (mfxU16)MSDK_ALIGN32(surface->Info.Height);
        *numBytes = surface->Data.PitchLow * Height2 * 2;
        linesize[0] = surface->Data.PitchLow;
        linesize[1] = surface->Data.PitchLow;
        linesize[2] = surface->Data.PitchLow;
        linesize[3] = 0;
        data[0] = surface->Data.Y;
        data[1] = surface->Data.U;
        data[2] = surface->Data.V;
        data[3] = nullptr;
        return 3;
    }

public:
    std::shared_ptr<MFXFrameAllocator> allocator;
    mfxFrameSurface1* surface;
    QAbstractVideoBuffer::MapMode mapMode;
};

class MfxQtVideoBuffer: public QAbstractVideoBuffer
{

    Q_DECLARE_PRIVATE(MfxQtVideoBuffer)
public:
    MfxQtVideoBuffer(
        mfxFrameSurface1* surface,
        const std::shared_ptr<MFXFrameAllocator>& allocator):
        QAbstractVideoBuffer(*(new MfxQtVideoBufferPrivate(surface, allocator)), NoHandle)
    {}
    virtual ~MfxQtVideoBuffer()
    {
    }

    virtual MapMode mapMode() const override
    {
        return d_func()->mapMode;
    }

    virtual uchar* map(MapMode /*mode*/, int *numBytes, int *bytesPerLine) override
    {
        Q_D(MfxQtVideoBuffer);

        d->allocator->LockFrame(d->surface->Data.MemId, &d->surface->Data);
        *bytesPerLine = d->surface->Data.PitchLow;
        *numBytes = GetSurfaceSize(
            d->surface->Info.FourCC,
            MSDK_ALIGN32(d->surface->Info.Width),
            MSDK_ALIGN32(d->surface->Info.Height));
        return d->surface->Data.Y;
    }

    virtual void unmap() override
    {
        Q_D(MfxQtVideoBuffer);
        d->allocator->UnlockFrame(d->surface->Data.MemId, &d->surface->Data);
    }
};

} // namespace nx::media::quick_sync
