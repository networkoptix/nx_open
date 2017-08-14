#pragma once

#include <plugins/metadata/abstract_iterable_metadata_packet.h>
#include <plugins/metadata/utils.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * @brief The Rect struct represents bounding box of detected object.
 */
struct Rect
{
    /**
     * @brief x coordinate of top left corner by x axis.
     * MUST be in the range [0..1].
     */
    float x = 0;

    /**
     * @brief y coordinate of top left corner by y axis.
     * MUST be in the range [0..1].
     */
    float y = 0;

    /**
     * @brief width of rectangle.
     * MUST be in the range [0..1] and the rule x + width <= 1 MUST be satisfied.
     */
    float width = 0;

    /**
     * @brief height of rectangle.
     * MUST be in the range [0..1] and the rule y + height <= 1 MUST be satisfied.
     */
    float height = 0;
};


/**
 * Each class that implements AbstractDetectedObject interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_DetectedObject
    = {{0x0f, 0xf4, 0xa4, 0x6f, 0xfd, 0x08, 0x4f, 0x4a, 0x97, 0x88, 0x16, 0xa0, 0x8c, 0xd6, 0x4a, 0x29}};

/**
 * @brief The AbstarctDetectedObject struct represents the single detected on the scene object.
 */
class AbstarctDetectedObject: public AbstractMetadataItem
{
public:
    /**
     * @brief id of detected object. If the object (e.g. particular person)
     * is detected on multiple frames this parameter SHOULD be the same every time.
     */
    virtual nxpl::NX_GUID id() = 0;

    /**
     * @brief (e.g. vehicle type: truck,  car, etc)
     */
    virtual NX_ASCII const char* objectSubType() = 0;

    /**
     * @brief attributes array of object attributes (e.g. age, color).
     */
    virtual NX_LOCALE_DEPENDENT Attribute* attributes() = 0;

    /**
     * @brief attributeCount count of attributes
     */
    virtual int attributeCount() = 0;

    /**
     * @brief auxilaryData user side data in json format. Null terminated UTF-8 string.
     */
    virtual const char* auxilaryData() = 0;

    /**
     * @brief boundingBox bounding box of detected object.
     */
    virtual Rect boundingBox() = 0;
};

/**
 * Each class that implements AbstractDetectionMetadataPacket interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_DetectionMetadataPacket
    = {{0x89, 0x89, 0xa1, 0x84, 0x72, 0x09, 0x4c, 0xde, 0xbb, 0x46, 0x09, 0xc1, 0x23, 0x2e, 0x31, 0x85}};

/**
 * @brief The AbstractDetectionMetadataPacket class is an interface for metadata packet
 * that contains the data about detected on the scene objects.
 */
class AbstractDetectionMetadataPacket: public AbstractIterableMetadataPacket
{
public:

    /**
     * @return next detected object or null if no more objects left.
     * This functions should not modify objects and behave like a constant iterator.
     */
    virtual AbstarctDetectedObject* nextItem() = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
