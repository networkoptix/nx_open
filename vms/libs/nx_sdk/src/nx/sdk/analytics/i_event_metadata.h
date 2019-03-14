#pragma once

#include <nx/sdk/interface.h>

#include "i_metadata.h"

namespace nx {
namespace sdk {
namespace analytics {

class IEventMetadata: public Interface<IEventMetadata, IMetadata>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IEventMetadata"); }

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
