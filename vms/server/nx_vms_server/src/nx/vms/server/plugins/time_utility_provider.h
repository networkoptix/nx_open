#pragma once

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_time_utility_provider.h>

namespace nx::vms::server::plugins {

class TimeUtilityProvider: public nx::sdk::RefCountable<nx::sdk::ITimeUtilityProvider>
{
public:
    virtual IRefCountable* queryInterface(InterfaceId id) override;

    virtual int64_t vmsSystemTimeSinceEpochMs() const override;
};

} // namespace nx::vms::server::plugins
