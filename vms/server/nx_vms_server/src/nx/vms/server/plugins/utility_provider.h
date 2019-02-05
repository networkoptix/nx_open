#pragma once

#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_utility_provider.h>
#include <nx/vms/server/plugins/time_utility_provider.h>

namespace nx::vms::server::plugins {

class UtilityProvider: public nx::sdk::RefCountable<nx::sdk::IUtilityProvider>
{
public:
    virtual IRefCountable* queryInterface(InterfaceId id) override
    {
        if (const auto ptr = IUtilityProvider::queryInterface(id))
            return ptr;
        if (const auto ptr = m_timeUtilityProvider->queryInterface(id))
            return ptr;
        return nullptr;
    }

private:
    nx::sdk::Ptr<TimeUtilityProvider> m_timeUtilityProvider{new TimeUtilityProvider()};
};

} // namespace nx::vms::server::plugins
