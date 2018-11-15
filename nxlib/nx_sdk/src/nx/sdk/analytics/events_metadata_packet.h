#pragma once

#include "iterable_metadata_packet.h"
#include "metadata_item.h"

namespace nx {
namespace sdk {
namespace analytics {

/** Each class implementing Event should handle this GUID in queryInterface(). */
static const nxpl::NX_GUID IID_Event =
  {{0xd5,0xe1,0x49,0x96,0x63,0x33,0x42,0x5a,0x8f,0xee,0xbc,0x23,0x50,0x03,0xc8,0x0f}};

class Event: public MetadataItem
{
public:
    /**
     * @return Null terminated UTF8 string containing the caption of the event.
     */
    virtual const char* caption() const = 0;

    /**
     * @return Null terminated UTF8 string containing the description of the event.
     */
    virtual const char* description() const = 0;

    /**
     * @brief auxilaryData user side data in json format. Null terminated UTF-8 string.
     */
    virtual const char* auxilaryData() const = 0;

    /**
     * @brief Is an event in active state.
     */
    virtual bool isActive() const = 0;
};

/** Each class implementing EventsMetadataPacket should handle this GUID in queryInterface(). */
static const nxpl::NX_GUID IID_EventsMetadataPacket =
  {{0x20,0xfc,0xa8,0x08,0x17,0x6b,0x48,0xa6,0x92,0xfd,0xba,0xb5,0x9d,0x37,0xd7,0xc0}};

/**
 * Interface for metadata packet containing information about some event.
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

} // namespace analytics
} // namespace sdk
} // namespace nx
