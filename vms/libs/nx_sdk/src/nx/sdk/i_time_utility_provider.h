#pragma once

#include <nx/sdk/i_utility_provider.h>

namespace nx {
namespace sdk {

/**
 * Provides time.
 *
 * NOTE: Is binary-compatible with the old SDK's nxpl::TimeProvider and supports its interface id.
 */
class ITimeUtilityProvider: public Interface<ITimeUtilityProvider, IUtilityProvider>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::ITimeUtilityProvider"); }

    /**
     * @return Synchronized System time - common time for all Servers in the VMS System.
     */
    virtual int64_t vmsSystemTimeSinceEpochMs() const = 0;
};

} // namespace sdk
} // namespace nx
