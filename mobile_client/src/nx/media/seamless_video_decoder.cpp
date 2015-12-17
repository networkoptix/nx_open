#include "seamless_video_decoder.h"
#include "video_decoder_registry.h"
#include "ffmpeg_decoder.h"

namespace nx
{
namespace media
{

bool SeamlessVideoDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QSharedPointer<QVideoFrame>* result)
{
	VideoDecoderRegistry::addPlugin<FfmpegDecoder>();
	return true;
}

}
}
