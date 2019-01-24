#pragma once

#include <nx/sdk/uuid.h>
#include <nx/sdk/i_attribute.h>
#include <nx/sdk/analytics/i_metadata.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements IObjectMetadata interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_ObjectMetadata =
    {{0x0f,0xf4,0xa4,0x6f,0xfd,0x08,0x4f,0x4a,0x97,0x88,0x16,0xa0,0x8c,0xd6,0x4a,0x29}};

/**
 * A single object detected on the scene.
 */
class IObjectMetadata: public IMetadata
{
public:
    /**
     * Bounding box of an object detected in a video frame.
     */
    struct Rect
    {
        Rect() = default;

        Rect(float x, float y, float width, float height):
            x(x), y(y), width(width), height(height)
        {
        }

        /**
         * X-coordinate of the top left corner. Must be in the range [0..1].
         */
        float x = 0;

        /**
         * Y-coordinate of the top left corner. Must be in the range [0..1].
         */
        float y = 0;

        /**
         * Width of the rectangle. Must be in the range [0..1], and the rule `x + width <= 1` must
         * be satisfied.
         */
        float width = 0;

        /**
         * Height of the rectangle. Must be in the range [0..1], and the rule `y + height <= 1`
         * must be satisfied.
         */
        float height = 0;
    };

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
     * Provides values of so-called Object Attributes - typically, some object properties (e.g. age or
     * color), represented as a name-value map.
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
