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
    double x = 0;

    /**
     * @brief y coordinate of top left corner by y axis.
     * MUST be in the range [0..1].
     */
    double y = 0;

    /**
     * @brief width of rectangle.
     * MUST be in the range [0..1] and the rule x + width <= 1 MUST be satisfied.
     */
    double width = 0;

    /**
     * @brief height of rectangle.
     * MUST be in the range [0..1] and the rule y + height <= 1 MUST be satisfied.
     */
    double height = 0;
};

/**
 * @brief The DetectedObject struct represents the single detected on the scene object.
 */
struct DetectedObject: public AbstractMetadataItem
{
    /**
     * @brief id of detected object. If the object (e.g. particular person)
     * is detected on multiple frames this parameter SHOULD be the same every time.
     */
    nxpl::NX_GUID id;


    /**
     * @brief (vehicle type | truck | car | e.t.c)
     */
    NX_ASCII char* objectSubType = nullptr;

    /**
     * @brief attributes array of object attributes (e.g. age, color).
     */
    NX_LOCALE_DEPENDENT Attribute* attributes = nullptr;

    /**
     * @brief attributeCount count of attributes
     */
    int attributeCount = 0;

    /**
     * @brief auxilaryData user side data in json format. Null terminated UTF-8 string.
     */
    char* auxilaryData = nullptr;

    /**
     * @brief boundingBox bounding box of detected object.
     */
    Rect boundingBox;
};

/**
 * Each class that implements AbstractDetectionMetadataPacket interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::GUID IID_DetectionMetadataPacket
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
    virtual const DetectedObject* nextItem() = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
