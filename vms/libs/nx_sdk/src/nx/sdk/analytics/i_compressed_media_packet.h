#pragma once

#include <cstdint>

#include <plugins/plugin_api.h>

#include "i_data_packet.h"
#include "i_media_context.h"

namespace nx {
namespace sdk {
namespace analytics {

using MediaFlags = uint64_t;

enum class MediaFlag: MediaFlags
{
    keyFrame = 1 << 0,
};

/** Each class implementing ICompressedMediaPacket should handle this GUID in queryInterface(). */
static const nxpl::NX_GUID IID_CompressedMediaPacket =
    {{0xf9,0xa4,0x59,0x8b,0xd7,0x18,0x42,0x29,0x98,0xdd,0xff,0xe5,0x41,0x28,0xfa,0xf8}};

/**
 * Interface for the packet containing compressed media data (audio or video).
 */
class ICompressedMediaPacket: public IDataPacket
{
public:
    /**
     * @return Null-terminated ASCII string containing the MIME type corresponding to the packet
     * data compression.
     */
    virtual const char* codec() const = 0;

    /**
     * @return Pointer to the compressed data.
     */
    virtual const char* data() const = 0;

    /**
     * @return Size of the compressed data in bytes.
     */
    virtual int dataSize() const = 0;

    /**
     * @return Pointer to the codec context.
     */
    virtual const IMediaContext* context() const = 0;

    /**
     * @return Packet media flags.
     */
    virtual MediaFlags flags() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
