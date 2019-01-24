#pragma once

#include "i_event_metadata.h"
#include "i_metadata_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/** Each class implementing IEventMetadataPacket should handle this GUID in queryInterface(). */
static const nxpl::NX_GUID IID_EventMetadataPacket =
  {{0x20,0xfc,0xa8,0x08,0x17,0x6b,0x48,0xa6,0x92,0xfd,0xba,0xb5,0x9d,0x37,0xd7,0xc0}};

/**
 * Metadata packet containing information about some event which occurred on the scene.
 */
class IEventMetadataPacket: public IMetadataPacket
{
public:
    virtual int count() const override = 0;

    virtual const IEventMetadata* at(int index) const override = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
