#pragma once

#include <QtCore/QScopedPointer>
#include <QtMultimedia/QAbstractVideoBuffer>

namespace nx {
namespace media {

/* 
* This class implements QT video buffer interface using aligned memory allocation.
* Some video decoders require aligned memory buffers.
*/
class AlignedMemVideoBufferPrivate;
class AlignedMemVideoBuffer: public QAbstractVideoBuffer
{
    Q_DECLARE_PRIVATE(AlignedMemVideoBuffer)
    QScopedPointer<AlignedMemVideoBufferPrivate> d_ptr;
public:
    AlignedMemVideoBuffer(int size, int alignFactor, int bytesPerLine);
    ~AlignedMemVideoBuffer();

    virtual MapMode mapMode() const override;

    virtual uchar *map(MapMode mode, int *numBytes, int *bytesPerLine) override;
    virtual void unmap() override;
};

}
}
