#pragma once

#include <QtCore/QScopedPointer>
#include <QtMultimedia/QAbstractVideoBuffer>

namespace nx {
namespace media {

/**
 * This class implements QT video buffer interface using aligned memory allocation.
 * Some video decoders require aligned memory buffers.
 */
class AlignedMemVideoBufferPrivate;
class AlignedMemVideoBuffer: public QAbstractVideoBuffer
{
    Q_DECLARE_PRIVATE(AlignedMemVideoBuffer)

public:
    AlignedMemVideoBuffer(int size, int alignFactor, int bytesPerLine);

    /**
     * Wrap an external buffer pointer, do not own the buffer.
     * ATTENTION: The caller is responsible to ensure buffer lifetime between map() and unmap().
     */
    AlignedMemVideoBuffer(uchar* data[4], int bytesPerLine[4], int planeCount);

    ~AlignedMemVideoBuffer();

    virtual MapMode mapMode() const override;

    virtual uchar* map(MapMode mode, int *numBytes, int *bytesPerLine) override;

    virtual void unmap() override;

protected:
    /**
     * NOTE: Qt has made QAbstractVideoBuffer::mapPlanes non-virtual, allowing only to override
     * its impl method QAbstractVideoBufferPrivate::map(). This method fixes this limitation:
     * it is internally called from AlignedMemVideoBufferPrivate::map() and can be overridden by
     * derived classes.
     */
    virtual int doMapPlanes(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]);
};

} // namespace media
} // namespace nx
