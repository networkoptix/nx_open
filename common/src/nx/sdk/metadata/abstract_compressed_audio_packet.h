#pragma once

#include <plugins/metadata/abstract_compressed_media_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractCompressedAudioPacket interface
 * should properly handle this GUID in its queryInterface method.
 */
static const nxpl::NX_GUID IID_CompressedAudio
    = {{0xca, 0x9d, 0x46, 0x91, 0x3f, 0xd0, 0x45, 0x8c, 0x9b, 0x1c, 0x98, 0x51, 0xe5, 0x6b, 0x85, 0xcc}};

/**
 * @brief The AbstractCompressedAudioPacket class is an interface for the packet
 * containing compressed audio data.
 */
class AbstractCompressedAudioPacket: public AbstractCompressedMediaPacket
{
public:
};

} // namespace metadata
} // namespace sdk
} // namespace nx
