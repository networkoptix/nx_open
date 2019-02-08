#include "aligned_mem_video_buffer.h"

#include <QtMultimedia/private/qabstractvideobuffer_p.h>
#include <nx/utils/log/assert.h>
QT_BEGIN_NAMESPACE
/**
 * This definition is needed because we inherit QAbstractVideoBufferPrivate class declared in a private Qt header.
 */
int QAbstractVideoBufferPrivate::map(
    QAbstractVideoBuffer::MapMode /*mode*/,
    int* /*numBytes*/,
    int /*bytesPerLine*/[4],
    uchar* /*data*/[4])
{
    NX_CRITICAL(false);
    return 0;
}

QT_END_NAMESPACE

namespace nx {
namespace media {

class AlignedMemVideoBufferPrivate: public QAbstractVideoBufferPrivate
{
    Q_DECLARE_PUBLIC(AlignedMemVideoBuffer)

public:
    AlignedMemVideoBufferPrivate():
        QAbstractVideoBufferPrivate(),
        mapMode(QAbstractVideoBuffer::NotMapped),
        dataSize(0),
        planeCount(1),
        ownBuffer(false)
    {
        memset(data, 0, sizeof(data));
        memset(bytesPerLine, 0, sizeof(bytesPerLine));
    }

    virtual ~AlignedMemVideoBufferPrivate() = default;

    virtual int map(
        QAbstractVideoBuffer::MapMode mode,
        int* numBytes,
        int bytesPerLine[4],
        uchar* data[4]) override
    {
        Q_Q(AlignedMemVideoBuffer);
        return q->doMapPlanes(mode, numBytes, bytesPerLine, data);
    }

    uchar* data[4];
    int bytesPerLine[4];
    QAbstractVideoBuffer::MapMode mapMode;
    int dataSize;
    int planeCount;
    bool ownBuffer;
};
AlignedMemVideoBuffer::AlignedMemVideoBuffer(int size, int alignFactor, int bytesPerLine):
    QAbstractVideoBuffer(*(new AlignedMemVideoBufferPrivate()), NoHandle)
{
    Q_D(AlignedMemVideoBuffer);
    d->data[0] = (uchar*) qMallocAligned(size, alignFactor);
    d->bytesPerLine[0] = bytesPerLine;
    d->dataSize = size;
    d->planeCount = 1;
    d->ownBuffer = true;
}
AlignedMemVideoBuffer::AlignedMemVideoBuffer(uchar* data[4], int bytesPerLine[4], int planeCount):
    QAbstractVideoBuffer(*(new AlignedMemVideoBufferPrivate()), NoHandle)
{
    Q_D(AlignedMemVideoBuffer);
    for (int i = 0; i < 4; ++i)
    {
        d->data[i] = data[i];
        d->bytesPerLine[i] = bytesPerLine[i];
    }
    d->dataSize = 0; //< not used
    d->planeCount = planeCount;
}

AlignedMemVideoBuffer::~AlignedMemVideoBuffer()
{
    Q_D(AlignedMemVideoBuffer);
    if (d->ownBuffer)
        qFreeAligned(d->data[0]);
}
AlignedMemVideoBuffer::MapMode AlignedMemVideoBuffer::mapMode() const
{
    return d_func()->mapMode;
}
uchar* AlignedMemVideoBuffer::map(MapMode mode, int* numBytes, int* bytesPerLine)
{
    Q_D(AlignedMemVideoBuffer);

    if (d->mapMode == NotMapped && d->data && mode != NotMapped)
    {
        d->mapMode = mode;

        if (numBytes)
            *numBytes = d->dataSize;

        if (bytesPerLine)
            *bytesPerLine = d->bytesPerLine[0];

        return d->data[0];
    }
    else
    {
        return nullptr;
    }
}

int AlignedMemVideoBuffer::doMapPlanes(
    MapMode mode, int* numBytes, int bytesPerLine[4], uchar* data[4])
{
    Q_UNUSED(mode);
    Q_D(AlignedMemVideoBuffer);

    for (int i = 0; i < 4; ++i)
    {
        data[i] = d->data[i];
        bytesPerLine[i] = d->bytesPerLine[i];
    }
    *numBytes = d->dataSize;
    return d->planeCount;
}

void AlignedMemVideoBuffer::unmap()
{
    Q_D(AlignedMemVideoBuffer);
    d->mapMode = NotMapped;
}

} // namespace media
} // namespace nx
