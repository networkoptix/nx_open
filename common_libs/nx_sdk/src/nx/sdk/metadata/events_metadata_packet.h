#pragma once

#include "iterable_metadata_packet.h"
#include "metadata_item.h"

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements Event interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_Event
    = {{0xd5, 0xe1, 0x49, 0x96, 0x63, 0x33, 0x42, 0x5a, 0x8f, 0xee, 0xbc, 0x23, 0x50, 0x03, 0xc8, 0x0f}};

class Event: public MetadataItem
{
public:
    /**
     * @return Null terminated UTF8 string containing the caption of the event.
     */
    virtual NX_LOCALE_DEPENDENT const char* caption() const = 0;

    /**
     * @return Null terminated UTF8 string containing the description of the event.
     */
    virtual NX_LOCALE_DEPENDENT const char* description() const = 0;

    /**
     * @brief auxilaryData user side data in json format. Null terminated UTF-8 string.
     */
    virtual const char* auxilaryData() const = 0;

    /**
     * @brief Is an event in active state.
     */
    virtual bool isActive() const = 0;
};

/**
* Each class that implements EventsMetadataPacket interface
* should properly handle this GUID in its queryInterface method
*/
static const nxpl::NX_GUID IID_EventsMetadataPacket
    = {{0x20, 0xfc, 0xa8, 0x08, 0x17, 0x6b, 0x48, 0xa6, 0x92, 0xfd, 0xba, 0xb5, 0x9d, 0x37, 0xd7, 0xc0}};

/**
 * @brief The EventsMetadataPacket class is an interface for metadata packet
 * containing information about some event.
 */
class EventsMetadataPacket: public IterableMetadataPacket
{
public:
    /**
     * Should not modify the object, and should behave like a constant iterator.
     * @return Next item in the list, or null if no more.
     */
    virtual Event* nextItem() = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
