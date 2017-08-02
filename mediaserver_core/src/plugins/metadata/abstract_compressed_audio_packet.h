#pragma once

#include <plugins/metadata/abstract_media_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::GUID IID_CompressedAudio
    = {{0xca, 0x9d, 0x46, 0x91, 0x3f, 0xd0, 0x45, 0x8c, 0x9b, 0x1c, 0x98, 0x51, 0xe5, 0x6b, 0x85, 0xcc}};

class AbstractCompressedAudioData: public AbstractCompressedMediaPacket
{
public:
};

} // namespace metadata
} // namespace sdk
} // namespace nx
