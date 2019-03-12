#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {
namespace analytics {

/** Provides various information about an Engine. */
class IEngineInfo: public Interface<IEngineInfo>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::IEngineInfo"); }

    /** @return Id assigned to this Engine by the Server. */
    virtual const char* id() const = 0;

    /** @return Name assigned to this Engine by the Server or by the user. */
    virtual const char* name() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
