// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_attribute.h>

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
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMetadata"); }

    /**
     * Human-readable hierarchical type, e.g. "someCompany.someEngine.lineCrossing".
     */
    virtual const char* typeId() const = 0;

    /**
     * Level of confidence in range (0..1]
     */
    virtual float confidence() const = 0;

    /**
     * Provides values of so-called Metadata Attributes - typically, some object or event
     * properties (e.g. age or color), represented as a name-value map.
     * @param index 0-based index of the attribute.
     * @return Item of an attribute array, or null if index is out of range.
     */
    protected: virtual const IAttribute* getAttribute(int index) const = 0;
    public: Ptr<const IAttribute> attribute(int index) const { return toPtr(getAttribute(index)); }

    /**
     * @return Number of items in the attribute array.
     */
    virtual int attributeCount() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
