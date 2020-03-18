#pragma once

#include <vector>

#include <nx/vms/api/data/analytics_data.h>

#include <nx/sdk/ptr.h>
#include <nx/sdk/i_ref_countable.h>

namespace nx::vms::server::metrics {

class PluginResourceBindingInfoHolder
{
public:
    virtual ~PluginResourceBindingInfoHolder() {}

    virtual std::vector<nx::vms::api::PluginResourceBindingInfo> bindingInfoForPlugin(
        const nx::sdk::Ptr<const nx::sdk::IRefCountable>& pluginRefCountable) const = 0;
};

class PluginResourceBindingInfoProvider
{
public:
    virtual ~PluginResourceBindingInfoProvider() {}

    virtual std::unique_ptr<PluginResourceBindingInfoHolder> bindingInfoHolder() const = 0;
};

} // namespace nx::vms::server::metrics
