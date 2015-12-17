#include "video_decoder_registry.h"

namespace nx
{
namespace media
{

VideoDecoderRegistry* VideoDecoderRegistry::instance()
{
	VideoDecoderRegistry instance;
	return &instance;
}

}
}
