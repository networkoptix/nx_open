#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>
#include <QOpenGLContext>

#include <nx/streaming/video_data_packet.h>

#include "media_fwd.h"

namespace nx
{
	namespace media
	{
		/*
		* Interface for video decoder implementation.
		*/
		class AbstractVideoDecoder: public QObject
		{
			Q_OBJECT
		public:
            AbstractVideoDecoder() {}
			virtual ~AbstractVideoDecoder() {}
            virtual void setAllocator(AbstractResourceAllocator* /*allocator */) {}
            virtual void setSharedGlContext(QOpenGLContext* context ) {}

			/*
			* \param context	codec context.
			* \returns true if decoder is compatible with provided context.
			* This function should be overrided despite static keyword. Otherwise it'll be compile error.
			*/
			static bool isCompatible(const QnConstCompressedVideoDataPtr& /* frame*/ ) { return false; }

			/*
			* Decode a video frame. This is a sync function and it could take a lot of CPU.
			* It's guarantee all source frames have same codec context.
			*
			* \param frame		compressed video data. If frame is null pointer then function must flush internal decoder buffer.
			* If no more frames in buffer left, function must returns true as result and null shared pointer in the 'result' parameter.
			* \param result		decoded video data. If decoder still fills internal buffer then result can be empty but function returns 0.
			* \!returns decoded frame number (value >=0)  if frame is decoded without errors. Returns negative value if decoding error.
			* For nullptr input data returns positive value while decoder is flushing internal buffer (result isn't null).
			*/
            virtual int decode(const QnConstCompressedVideoDataPtr& frame, QnVideoFramePtr* result = nullptr) = 0;
		};
	}
}
