#pragma once

#include <cstdint>

#include <plugins/plugin_api.h>

#include "data_packet.h"
#include "media_context.h"

namespace nx {
namespace sdk {
namespace metadata {

using MediaFlags = uint64_t;

enum class MediaFlag: MediaFlags
{
    keyFrame = 1 << 0,
};

/** Each class implementing CompressedMediaPacket should handle this GUID in queryInterface(). */
static const nxpl::NX_GUID IID_CompressedMediaPacket =
    {{0xf9,0xa4,0x59,0x8b,0xd7,0x18,0x42,0x29,0x98,0xdd,0xff,0xe5,0x41,0x28,0xfa,0xf8}};

/**
 * Interface for the packet containing compressed media data (audio or video).
 */
class CompressedMediaPacket: public DataPacket
{
    using base_type = DataPacket;

public:
    /**
     * @return Null-terminated ASCII string containing the MIME type corresponding to the packet
     * data compression.
     */
    virtual const char* codec() const = 0;

    /**
     * @return Pointer to compressed data.
     */
    virtual const char* data() const = 0;

    /**
     * @return Size of compressed data in bytes.
     */
    virtual const int dataSize() const = 0;

    /**
     * @return Pointer to the codec context.
     */
    virtual const MediaContext* context() const = 0;

    /**
     * @return Packet media flags.
     */
    virtual MediaFlags flags() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
