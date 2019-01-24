#pragma once

#include "i_object_metadata.h"
#include "i_metadata_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements IObjectMetadataPacket interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_ObjectMetadataPacket =
    {{0x89,0x89,0xa1,0x84,0x72,0x09,0x4c,0xde,0xbb,0x46,0x09,0xc1,0x23,0x2e,0x31,0x85}};

/**
 * Metadata packet that contains data about objects detected on the scene.
 */
class IObjectMetadataPacket: public IMetadataPacket
{
public:
    virtual int count() const override = 0;

    virtual const IObjectMetadata* at(int index) const override = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
