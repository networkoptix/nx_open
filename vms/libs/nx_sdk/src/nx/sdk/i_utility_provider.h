#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {

/**
 * Represents an object which the plugin can use for calling back to access some data and
 * functionality provided by the process that uses the plugin.
 *
 * To use this object, request an object implementing a particular I...UtilityProvider via
 * queryInterface(). All such interfaces in the current SDK version are supported, but if a plugin
 * intends to support VMS versions using some older SDK, it should be ready to accept the denial.
 */
class IUtilityProvider: public Interface<IUtilityProvider>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::IUtilityProvider"); }
};

} // namespace sdk
} // namespace nx
