// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtMultimedia/QAbstractVideoBuffer>

#include <nx/utils/impl_ptr.h>

namespace nx {
namespace media {

/**
 * This class implements QT video buffer interface using aligned memory allocation.
 * Some video decoders require aligned memory buffers.
 */
class AlignedMemVideoBufferPrivate;
class AlignedMemVideoBuffer: public QAbstractVideoBuffer
{
public:
    AlignedMemVideoBuffer(int size, int alignFactor, int bytesPerLine);

    /**
     * Wrap an external buffer pointer, do not own the buffer.
     * ATTENTION: The caller is responsible to ensure buffer lifetime between map() and unmap().
     */
    AlignedMemVideoBuffer(uchar* data[4], int bytesPerLine[4], int planeCount);

    ~AlignedMemVideoBuffer();

    virtual MapData map(QVideoFrame::MapMode mode) override;

    virtual void unmap() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace media
} // namespace nx
