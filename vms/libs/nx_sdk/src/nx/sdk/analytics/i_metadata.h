#pragma once

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements IMetadata interface should properly handle this GUID in its
 * queryInterface() method.
 */
static const nxpl::NX_GUID IID_Metadata =
    {{0xb3,0x23,0x89,0x1d,0x19,0x62,0x44,0x3c,0x84,0x2a,0x07,0x50,0x7d,0x87,0xab,0x4e}};

/**
 * A particular item of metadata (e.g. event, detected object) which is contained in a metadata
 * packet.
 */
class IMetadata: public nxpl::PluginInterface
{
public:
    /**
     * Human-readable hierarchical type, e.g. "someCompany.someEngine.lineCrossing".
     */
    virtual const char* typeId() const = 0;

    /**
     * Level of confidence in range (0..1]
     */
    virtual float confidence() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
