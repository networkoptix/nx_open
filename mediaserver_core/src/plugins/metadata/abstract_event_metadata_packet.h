#pragma once

#include <plugins/metadata/abstract_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractEventMetadataPacket interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::GUID IID_EventMetadataPacket
    = {{0x20, 0xfc, 0xa8, 0x08, 0x17, 0x6b, 0x48, 0xa6, 0x92, 0xfd, 0xba, 0xb5, 0x9d, 0x37, 0xd7, 0xc0}};

struct AnalyticsEvent: public AbstractMetadataItem
{
    /**
    * @brief Human readable object type (line crossing | human detected | etc)
    */
    NX_ASCII char* eventType;

    /**
     * @return Null terminated UTF8 string containing the caption of the event.
     */
    NX_LOCALE_DEPENDENT char* caption;

    /**
     * @return Null terminated UTF8 string containing the description of the event.
     */
    NX_LOCALE_DEPENDENT char* description;

    /**
     * @brief auxilaryData user side data in json format. Null terminated UTF-8 string.
     */
    char* auxilaryData = nullptr;
};

/**
 * @brief The AbstractEventMetadataPacket class is an interface for metadata packet
 * containing information about some event.
 */
class AbstractEventMetadataPacket: public AbstractMetadataPacket
{
    /**
    * @return next detected object or null if no more objects left.
    * This functions should not modify objects and behave like a constant iterator.
    */
    virtual const AnalyticsEvent* nextObject() = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
