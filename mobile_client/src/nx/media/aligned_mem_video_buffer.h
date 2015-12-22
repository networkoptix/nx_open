#pragma once

#include <QtMultimedia/qabstractvideobuffer.h>

namespace nx
{
	namespace media
	{
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
