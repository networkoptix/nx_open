#pragma once

#include <cstdint>

#include "i_data_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements IMetadataPacket interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_MetadataPacket =
    {{0x28,0xbb,0xe1,0x4e,0xda,0xea,0x48,0xc9,0xb9,0x14,0xfb,0x07,0xde,0x28,0x6c,0xbf}};

/**
 * Interface for packets containing metadata (e.g. events, object detections).
 */
class IMetadataPacket: public IDataPacket
{
public:
    /**
     * @return Validity duration of the metadata in the packet, or 0 if irrelevant.
     */
    virtual int64_t durationUs() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
