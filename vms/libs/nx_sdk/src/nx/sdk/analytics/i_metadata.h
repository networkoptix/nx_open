#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * A particular item of metadata (e.g. event, detected object) which is contained in a metadata
 * packet.
 */
class IMetadata: public Interface<IMetadata>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IMetadata"); }

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
