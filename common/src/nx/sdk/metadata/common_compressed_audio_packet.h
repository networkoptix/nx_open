#pragma once

#include "compressed_audio_packet.h"
#include "common_compressed_media_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

class CommonCompressedAudioPacket: public CommonCompressedMediaPacket<CompressedAudioPacket>
{
};

} // namespace metadata
} // namespace sdk
} // namespace nx
