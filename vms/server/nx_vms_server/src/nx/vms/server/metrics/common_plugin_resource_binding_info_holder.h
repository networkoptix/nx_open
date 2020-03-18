#pragma once

#include <map>

#include <nx/vms/server/metrics/plugin_resource_binding_info_provider.h>

#include <nx/vms/server/resource/resource_fwd.h>

namespace nx::vms::server::metrics {

class CommonPluginResourceBindingInfoHolder: public PluginResourceBindingInfoHolder
{
public:
    using DevicePluginBindingInfoMap = std::map<
        const nx::sdk::Ptr<const nx::sdk::IRefCountable>,
        nx::vms::api::PluginResourceBindingInfo>;

    using EngineBindingInfoMap = std::map<
        resource::AnalyticsEngineResourcePtr,
        nx::vms::api::PluginResourceBindingInfo>;

public:
    CommonPluginResourceBindingInfoHolder(EngineBindingInfoMap bindingInfoByEngine);

    CommonPluginResourceBindingInfoHolder(DevicePluginBindingInfoMap bindingInfoByPlugin);

    virtual std::vector<nx::vms::api::PluginResourceBindingInfo> bindingInfoForPlugin(
        const nx::sdk::Ptr<const nx::sdk::IRefCountable>& pluginRefCountable) const override;

private:
    std::map<
        const nx::sdk::Ptr<const nx::sdk::IRefCountable>,
        std::vector<nx::vms::api::PluginResourceBindingInfo>> m_bindingInfoByPlugin;
};

} // namespace nx::vms::server::metrics
