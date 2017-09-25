#pragma once

#include <cstdint>

#include <nx/sdk/metadata/abstract_data_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractMetadataPacket interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_MetadataPacket =
    {{0x28, 0xbb, 0xe1, 0x4e, 0xda, 0xea, 0x48, 0xc9, 0xb9, 0x14, 0xfb, 0x07, 0xde, 0x28, 0x6c, 0xbf}};

/**
 * @brief The AbstractMetadataPacket class is an interface for packets
 * containing metadata (e.g. events, object detections).
 */
class AbstractMetadataPacket: public AbstractDataPacket
{
public:
    virtual int64_t timestampUsec() const = 0;

    /**
     * @return duration of packet or zero if not used.
     */
    virtual int64_t durationUsec() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
