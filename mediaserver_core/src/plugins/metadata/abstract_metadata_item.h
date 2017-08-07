#pragma once

#include <plugins/plugin_api.h>
#include <plugins/metadata/abstract_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractMetadataItem interface
 * should properly handle this GUID in its queryInterface method.
 */
static const nxpl::GUID IID_MetadataItem
    = {{0xb3, 0x23, 0x89, 0x1d, 0x19, 0x62, 0x44, 0x3c, 0x84, 0x2a, 0x07, 0x50, 0x7d, 0x87, 0xab, 0x4e}};

/**
 * @brief The AbstractMetadataItem class is an interface for particular
 * item of metadata (e.g. event, detected object) which is contained in a metadata packet.
 */
class AbstractMetadataItem: public nxpl::PluginInterface
{
public:
    /**
    * @brief Human readable object type (line crossing | human detected | etc)
    */
    GUID eventTypeId;

    /**
    * @brief Level of confidence in range (0..1]
    */
    double confidence = 1.0;
};

} // namespace metadata
} // namesapce sdk
} // namespace nx
