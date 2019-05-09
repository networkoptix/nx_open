#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/uuid.h>
#include <nx/sdk/i_attribute.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/i_metadata.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * A single object detected on the scene.
 */
class IObjectMetadata: public Interface<IObjectMetadata, IMetadata>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IObjectMetadata"); }

    /**
     * Id of the object. If the object (e.g. a particular person) is detected on multiple frames,
     * this parameter should be the same each time.
     */
    virtual Uuid id() const = 0;

    /**
     * @return Subclass of the object (e.g. vehicle type: truck, car, etc.).
     */
    virtual const char* subtype() const = 0;

    /**
     * Provides values of so-called Object Attributes - typically, some object properties (e.g. age
     * or color), represented as a name-value map.
     * @param index 0-based index of the attribute.
     * @return Item of an attribute array, or null if index is out of range.
     */
    virtual const IAttribute* attribute(int index) const = 0;

    /**
     * @return Number of items in the attribute array.
     */
    virtual int attributeCount() const = 0;

    /**
     * Arbitrary data (in json format) associated with the object.
     * @return JSON string in UTF-8.
     */
    virtual const char* auxiliaryData() const = 0;

    /**
     * @return Bounding box of an object detected in a video frame.
     */
    virtual Rect boundingBox() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
