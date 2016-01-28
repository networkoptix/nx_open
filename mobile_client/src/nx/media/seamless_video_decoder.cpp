#include "seamless_video_decoder.h"
#include "ffmpeg_decoder.h"

namespace nx
{
namespace media
{

bool SeamlessVideoDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QSharedPointer<QVideoFrame>* result)
{
	return true;
}

}
}
