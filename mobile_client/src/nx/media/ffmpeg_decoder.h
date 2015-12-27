#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx
{
	namespace media
	{
		/*
		* This class implements ffmpeg video decoder
		*/
		class FfmpegDecoderPrivate;
		class FfmpegDecoder : public AbstractVideoDecoder
		{
		public:
			FfmpegDecoder();
			virtual ~FfmpegDecoder();

			static bool isCompatible(const QnConstCompressedVideoDataPtr& frame);
            virtual int decode(const QnConstCompressedVideoDataPtr& frame, QnVideoFramePtr* result = nullptr) override;
		private:
			QScopedPointer<FfmpegDecoderPrivate> d_ptr;
			Q_DECLARE_PRIVATE(FfmpegDecoder);
		};
	}
}
