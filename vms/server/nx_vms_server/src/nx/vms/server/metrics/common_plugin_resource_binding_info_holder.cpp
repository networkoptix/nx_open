#include "common_plugin_resource_binding_info_holder.h"

#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/analytics/wrappers/plugin.h>

namespace nx::vms::server::metrics {

using namespace nx::sdk;
using namespace nx::vms::server::analytics;

CommonPluginResourceBindingInfoHolder::CommonPluginResourceBindingInfoHolder(
    EngineBindingInfoMap bindingInfoByEngine)
{
    for (auto& [engine, bindingInfo]: bindingInfoByEngine)
    {
        if (!NX_ASSERT(engine))
            continue;

        const auto plugin = engine->plugin()
            .dynamicCast<nx::vms::server::resource::AnalyticsPluginResource>();

        if (!NX_ASSERT(plugin))
            continue;

        const wrappers::PluginPtr pluginWrapper = plugin->sdkPlugin();
        if (!NX_ASSERT(pluginWrapper))
            continue;

        const Ptr<IPlugin> sdkPlugin = pluginWrapper->sdkObject();
        if (!NX_ASSERT(sdkPlugin))
            continue;

        if (auto refCountable = sdkPlugin->queryInterface<const IRefCountable>())
            m_bindingInfoByPlugin[std::move(refCountable)].push_back(std::move(bindingInfo));
    }
}

CommonPluginResourceBindingInfoHolder::CommonPluginResourceBindingInfoHolder(
    DevicePluginBindingInfoMap bindingInfoByPlugin)
{
    for (auto& [pluginRefCountable, bindingInfo]: bindingInfoByPlugin)
        m_bindingInfoByPlugin[pluginRefCountable].push_back(std::move(bindingInfo));
}

std::vector<nx::vms::api::PluginResourceBindingInfo>
    CommonPluginResourceBindingInfoHolder::bindingInfoForPlugin(
        const nx::sdk::Ptr<const nx::sdk::IRefCountable>& pluginRefCountable) const
{
    if (const auto it = m_bindingInfoByPlugin.find(pluginRefCountable);
        it != m_bindingInfoByPlugin.cend())
    {
        return it->second;
    }

    return {};
}

} // namespace nx::vms::server::metrics
