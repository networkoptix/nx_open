#pragma once

#include <nx/sdk/uuid.h>
#include <nx/sdk/i_attribute.h>
#include <nx/sdk/analytics/i_metadata_item.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements IObject interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_Object =
    {{0x0f,0xf4,0xa4,0x6f,0xfd,0x08,0x4f,0x4a,0x97,0x88,0x16,0xa0,0x8c,0xd6,0x4a,0x29}};

/**
 * Represents a single object detected on the scene.
 */
class IObject: public IMetadataItem
{
public:
    /**
     * Bounding box of an analytics object.
     */
    struct Rect
    {
        Rect() {}

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

    // TODO: #mshevchenko: Fix comments.

    // TODO: #mshevchenko: Rename to subtype.
    /**
     * @brief (e.g. vehicle type: truck,  car, etc)
     */
    virtual const char* objectSubType() const = 0;

    /**
     * @brief attributes array of object attributes (e.g. age, color).
     */
    virtual const IAttribute* attribute(int index) const = 0;

    /**
     * @brief attributeCount count of attributes
     */
    virtual int attributeCount() const = 0;

    /**
     * @brief auxiliaryData user side data in json format. Null terminated UTF-8 string.
     */
    virtual const char* auxiliaryData() const = 0;

    /**
     * @brief boundingBox bounding box of detected object.
     */
    virtual Rect boundingBox() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
