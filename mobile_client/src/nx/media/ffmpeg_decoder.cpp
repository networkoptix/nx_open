#include "ffmpeg_decoder.h"
#include "video_decoder_registry.h"

namespace nx
{
namespace media
{

bool FfmpegDecoder::isCompatible(const QnConstCompressedVideoDataPtr& frame)
{
	// todo: implement me
	return false;
}

bool FfmpegDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QSharedPointer<QVideoFrame>* result)
{
	// todo: implement me
	VideoDecoderRegistry::instance()->addPlugin<FfmpegDecoder>();
	return false;
}

}
}
