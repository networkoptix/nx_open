#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

namespace nx
{
	namespace media
	{
		/*
		* This class encapsulate common logic related to any video decoder. It guarantee seamless decoding in case of compressed frame
		* has changed resolution or codecId. VideoDecoder uses PhysicalDecoderFactory to instantiate compatible PhysicalDecoder to decode next frame
		* if video parameters have changed.
		*/
		class SeamlessVideoDecoderPrivate;
		class SeamlessVideoDecoder
		{
		public:

			/*
			* Decode a video frame. This is a sync function and it could take a lot of CPU. This isn't thread safe call.
			*
			* \param frame		compressed video data. If frame is null pointer then function must flush internal decoder buffer.
			* If no more frames in buffer left, function must returns true as result and null shared pointer in the 'result' parameter.
			* \param result		decoded video data. If decoder still fills internal buffer then result can be empty but function return true.
			* \!returns true if frame is decoded without errors. For nullptr input data returns true while flushing internal buffer (result isn't null)
			*/
			bool decode(const QnConstCompressedVideoDataPtr& frame, QSharedPointer<QVideoFrame>* result = nullptr);

		private:
			QScopedPointer<SeamlessVideoDecoderPrivate> d_ptr;
			Q_DECLARE_PRIVATE(SeamlessVideoDecoder);
		};
	}
}
