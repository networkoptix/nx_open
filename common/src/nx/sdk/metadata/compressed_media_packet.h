#pragma once

#include <plugins/plugin_api.h>
#include <plugins/metadata/data_packet.h>
#include <plugins/metadata/media_context.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements CompressedMediaPacket interface
 * should properly handle this GUID in its queryInterface method.
 */
static const nxpl::NX_GUID IID_CompressedMediaPacket
    = {{0xf9, 0xa4, 0x59, 0x8b, 0xd7, 0x18, 0x42, 0x29, 0x98, 0xdd, 0xff, 0xe5, 0x41, 0x28, 0xfa, 0xf8}};

/**
 * @brief The CompressedMediaPacket class is an interface for the packet
 * containing compressed media data (audio or video).
 */
class CompressedMediaPacket: public DataPacket
{
public:
    /**
     * @return Null terminated ASCII string containing
     * MIME type corresponding to the packet data compression.
     */
    virtual const char* codec() const = 0;

    /**
     * @return pointer to compressed data.
     */
    virtual const char* data() const = 0;

    /**
     * @return size of compressed data in bytes.
     */
    virtual const int dataSize() const = 0;

    /**
     * @return pointer to the codec context.
     */
    virtual const MediaContext* context() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
