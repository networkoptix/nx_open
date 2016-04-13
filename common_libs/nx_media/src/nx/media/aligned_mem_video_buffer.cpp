#include "aligned_mem_video_buffer.h"

#include <utils/common/qt_private_headers.h>
#include QT_PRIVATE_HEADER(QtMultimedia,qabstractvideobuffer_p.h)

QT_BEGIN_NAMESPACE


int QAbstractVideoBufferPrivate::map(
    QAbstractVideoBuffer::MapMode mode,
    int *numBytes,
    int bytesPerLine[4],
    uchar *data[4])
{
    assert(false);
    return 0;
}

QT_END_NAMESPACE

namespace nx
{
namespace media
{


class AlignedMemVideoBufferPrivate: public QAbstractVideoBufferPrivate
{
public:
    AlignedMemVideoBufferPrivate():
        QAbstractVideoBufferPrivate(),
        mapMode(QAbstractVideoBuffer::NotMapped),
        dataSize(0),
        planeCount(1),
        ownBuffer(false)
    {
        memset(data, 0, sizeof(dataSize));
        memset(bytesPerLine, 0, sizeof(bytesPerLine));
    }

    virtual ~AlignedMemVideoBufferPrivate() {
        int gg = 4;
    }

    virtual int map(
        QAbstractVideoBuffer::MapMode mode,
        int *numBytes,
        int bytesPerLine[4],
        uchar *data[4]) override
    {
        for (int i = 0; i < 4; ++i)
        {
            data[i] = this->data[i];
            bytesPerLine[i] = this->bytesPerLine[i];
        }
        *numBytes = dataSize;
        return planeCount;
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

uchar *AlignedMemVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    Q_D(AlignedMemVideoBuffer);

    if (d->mapMode == NotMapped && d->data && mode != NotMapped) 
    {
        d->mapMode = mode;

        if (numBytes)
            *numBytes = d->dataSize;

        if (bytesPerLine)
            *bytesPerLine = d->bytesPerLine[0];

        return reinterpret_cast<uchar *>(d->data);
    }
    else {
        return nullptr;
    }
}

void AlignedMemVideoBuffer::unmap()
{
    d_func()->mapMode = NotMapped;
}

}
}
