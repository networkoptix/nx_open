#pragma once

#include <plugins/plugin_api.h>
#include "abstract_compressed_media_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractCompressedVideoPacket interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_CompressedVideoPacket
    = {{0xb6, 0x39, 0xe4, 0x68, 0x0d, 0x95, 0x49, 0x76, 0xa7, 0xc3, 0x68, 0x4b, 0xcc, 0x4d, 0x90, 0xb9}};

/**
 * @brief The AbstractCompressedVideoPacket class is an interface for the packet
 * containing compressed video data.
 */
class AbstractCompressedVideoPacket: public AbstractCompressedMediaPacket
{
    using base_type = AbstractCompressedMediaPacket;
public:
    /**
     * @return width of video frame in pixels.
     */
    virtual int width() const = 0;

    /**
     * @return height of video frame in pixels.
     */
    virtual int height() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
