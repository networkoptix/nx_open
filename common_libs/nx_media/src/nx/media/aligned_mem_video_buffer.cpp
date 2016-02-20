#include "aligned_mem_video_buffer.h"

#include <utils/common/qt_private_headers.h>
#include QT_PRIVATE_HEADER(QtMultimedia,qabstractvideobuffer_p.h)

namespace nx
{
namespace media
{

class AlignedMemVideoBufferPrivate
{
public:
    AlignedMemVideoBufferPrivate():
        bytesPerLine(0),
        mapMode(QAbstractVideoBuffer::NotMapped),
        data(nullptr),
        dataSize(0)
    {
    }

    int bytesPerLine;
    QAbstractVideoBuffer::MapMode mapMode;
    uchar* data;
    int dataSize;
};

AlignedMemVideoBuffer::AlignedMemVideoBuffer(int size, int alignFactor, int bytesPerLine):
    QAbstractVideoBuffer(NoHandle),
    d_ptr(new AlignedMemVideoBufferPrivate())
{
    Q_D(AlignedMemVideoBuffer);
    d->data = (uchar*) qMallocAligned(size, alignFactor);
    d->dataSize = size;
    d->bytesPerLine = bytesPerLine;
}

AlignedMemVideoBuffer::~AlignedMemVideoBuffer()
{
    Q_D(AlignedMemVideoBuffer);
    qFreeAligned(d->data);
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
            *bytesPerLine = d->bytesPerLine;

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
