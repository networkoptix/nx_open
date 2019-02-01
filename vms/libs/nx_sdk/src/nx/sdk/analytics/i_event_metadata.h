#pragma once

#include "i_metadata.h"

namespace nx {
namespace sdk {
namespace analytics {

/** Each class implementing IEventMetadata should handle this GUID in queryInterface(). */
static const nxpl::NX_GUID IID_EventMetadata =
  {{0xd5,0xe1,0x49,0x96,0x63,0x33,0x42,0x5a,0x8f,0xee,0xbc,0x23,0x50,0x03,0xc8,0x0f}};

class IEventMetadata: public IMetadata
{
public:
    /**
     * @return Caption of the event, in UTF-8.
     */
    virtual const char* caption() const = 0;

    /**
     * @return Description of the event, in UTF-8.
     */
    virtual const char* description() const = 0;

    /**
     * @return User-side data in json format, in UTF-8.
     */
    virtual const char* auxiliaryData() const = 0;

    /**
     * @return Whether the event is in active state.
     */
    virtual bool isActive() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
