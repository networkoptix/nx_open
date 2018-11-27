#pragma once

#include <nx/sdk/common.h>

#include "iterable_metadata_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Represents bounding box of an analytics object.
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
     * Width of the rectangle. Must be in the range [0..1], and the rule `x + width <= 1` must be
     * satisfied.
     */
    float width = 0;

    /**
     * Height of the rectangle. Must be in the range [0..1], and the rule `y + height <= 1` must be
     * satisfied.
     */
    float height = 0;
};

/**
 * Each class that implements Object interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_Object
    = {{0x0f, 0xf4, 0xa4, 0x6f, 0xfd, 0x08, 0x4f, 0x4a, 0x97, 0x88, 0x16, 0xa0, 0x8c, 0xd6, 0x4a, 0x29}};

/**
 * Represents a single object detected on the scene.
 */
class Object: public MetadataItem
{
public:
    /**
     * @brief id of detected object. If the object (e.g. particular person)
     * is detected on multiple frames this parameter SHOULD be the same every time.
     */
    virtual nxpl::NX_GUID id() const = 0;

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
     * @brief auxilaryData user side data in json format. Null terminated UTF-8 string.
     */
    virtual const char* auxilaryData() const = 0;

    /**
     * @brief boundingBox bounding box of detected object.
     */
    virtual Rect boundingBox() const = 0;
};

/**
 * Each class that implements ObjectsMetadataPacket interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_ObjectsMetadataPacket
    = {{0x89, 0x89, 0xa1, 0x84, 0x72, 0x09, 0x4c, 0xde, 0xbb, 0x46, 0x09, 0xc1, 0x23, 0x2e, 0x31, 0x85}};

/**
 * Interface for metadata packet that contains data about objects detected on the scene.
 */
class ObjectsMetadataPacket: public IterableMetadataPacket
{
public:
    /**
     * Should not modify the object, and should behave like a constant iterator.
     * @return Next item in the list, or null if no more.
     */
    virtual Object* nextItem() = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
