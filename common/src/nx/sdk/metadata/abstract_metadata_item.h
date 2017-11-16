#pragma once

#include <plugins/plugin_api.h>
#include <nx/sdk/metadata/abstract_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractMetadataItem interface should properly handle this GUID in
 * its queryInterface() method.
 */
static const nxpl::NX_GUID IID_MetadataItem
    = {{0xb3, 0x23, 0x89, 0x1d, 0x19, 0x62, 0x44, 0x3c, 0x84, 0x2a, 0x07, 0x50, 0x7d, 0x87, 0xab, 0x4e}};

/**
 * Interface for particular item of metadata (e.g. event, detected object) which is contained in a 
 * metadata packet.
 */
class AbstractMetadataItem: public nxpl::PluginInterface
{
public:
    // TODO: #mike: Rename.
    /**
    * @brief Human readable object type (line crossing | human detected | etc)
    */
    virtual nxpl::NX_GUID eventTypeId() const = 0;

    /**
    * @brief Level of confidence in range (0..1]
    */
    virtual float confidence() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
