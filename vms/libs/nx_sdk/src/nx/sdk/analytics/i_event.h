#pragma once

#include "i_metadata_item.h"

namespace nx {
namespace sdk {
namespace analytics {

/** Each class implementing IEvent should handle this GUID in queryInterface(). */
static const nxpl::NX_GUID IID_Event =
  {{0xd5,0xe1,0x49,0x96,0x63,0x33,0x42,0x5a,0x8f,0xee,0xbc,0x23,0x50,0x03,0xc8,0x0f}};

class IEvent: public IMetadataItem
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
     * @brief auxiliaryData user side data in json format. Null terminated UTF-8 string.
     */
    virtual const char* auxiliaryData() const = 0;

    /**
     * @brief Is an event in active state.
     */
    virtual bool isActive() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
