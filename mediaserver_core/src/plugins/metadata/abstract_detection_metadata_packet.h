#pragma once

#include <plugins/metadata/abstract_metadata_packet.h>
#include <plugins/metadata/utils.h>

namespace nx {
namespace sdk {
namespace metadata {

static const char* kObjectTypeAttribute = "objectType";
static const char* kObjectClassAttribute = "objectClass";

static const char* kPersonObjectType = "person";
static const char* kVehicleObjectType = "vehicle";

/**
 * @brief The Rect struct reprsents bounding box of detected object.
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
 * @brief The DetectedObject struct resprsents the single detected on the scene object.
 */
struct DetectedObject
{
    /**
     * @brief id of detected object. If the object (e.g. particular person)
     * is detected on multiple frames this parameter SHOULD be the same every time.
     */
    nxpl::NX_GUID id;

    /**
     * @brief attributes array of object attributes (e.g. age, color).
     */
    NX_LOCALE_DEPENDENT Attribute* attributes = nullptr;

    /**
     * @brief attributeCount count of attributes
     */
    int attributeCount = 0;

    /**
     * @brief auxilaryData some side data in user defined format.
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
class AbstractDetectionMetadataPacket: public AbstractMetadataPacket
{
public:

    /**
     * @brief detectionInfo retrieves information about detected objects.
     * @param detectedObjectsBuffer pointer to the output buffer.
     * @param inOutMaxDetectedObjects size of output buffer. If this size is not enough then
     * this method MUST set this parameter to the desired size and return needMoreBufferSpace.
     * @return noError in case of success,
     * needMoreBufferSpace if size of buffer too small, some other value otherwise.
     */
    virtual Error detectionInfo(
        DetectedObject* detectedObjectsBuffer,
        int* inOutMaxDetectedObjects) = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
