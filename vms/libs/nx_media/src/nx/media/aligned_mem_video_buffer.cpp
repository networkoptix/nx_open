// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aligned_mem_video_buffer.h"


namespace nx {
namespace media {

struct AlignedMemVideoBuffer::Private
{
    uchar* data[4] = {};
    int bytesPerLine[4] = {};
    QVideoFrame::MapMode mapMode = QVideoFrame::MapMode::NotMapped;
    int dataSize = 0;
    int planeCount = 1;
    bool ownBuffer = false;
};

AlignedMemVideoBuffer::AlignedMemVideoBuffer(int size, int alignFactor, int bytesPerLine):
    QAbstractVideoBuffer(QVideoFrame::NoHandle),
    d(new AlignedMemVideoBuffer::Private())
{
    d->data[0] = (uchar*) qMallocAligned(size, alignFactor);
    d->bytesPerLine[0] = bytesPerLine;
    d->dataSize = size;
    d->planeCount = 1;
    d->ownBuffer = true;
}

AlignedMemVideoBuffer::AlignedMemVideoBuffer(uchar* data[4], int bytesPerLine[4], int planeCount):
    QAbstractVideoBuffer(QVideoFrame::NoHandle),
    d(new AlignedMemVideoBuffer::Private())
{
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
    if (d->ownBuffer)
        qFreeAligned(d->data[0]);
}

QAbstractVideoBuffer::MapData AlignedMemVideoBuffer::map(QVideoFrame::MapMode mode)
{
    MapData data;

    if (d->mapMode == QVideoFrame::NotMapped && mode != QVideoFrame::NotMapped)
    {
        d->mapMode = mode;

        for (int i = 0; i < 4; ++i)
        {
            data.data[i] = d->data[i];
            data.bytesPerLine[i] = d->bytesPerLine[i];
        }

        data.size[0] = d->dataSize;

        data.nPlanes = d->planeCount;
    }

    return data;
}

void AlignedMemVideoBuffer::unmap()
{
    d->mapMode = QVideoFrame::NotMapped;
}

} // namespace media
} // namespace nx
