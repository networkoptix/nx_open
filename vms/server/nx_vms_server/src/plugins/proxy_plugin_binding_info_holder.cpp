#include "proxy_plugin_binding_info_holder.h"

ProxyPluginBindingInfoHolder::ProxyPluginBindingInfoHolder(
    BindingInfoHolderList bindingInfoHolderList)
    :
    m_proxiedBindingInfoHolders(std::move(bindingInfoHolderList))
{
}

std::vector<nx::vms::api::PluginResourceBindingInfo>
    ProxyPluginBindingInfoHolder::bindingInfoForPlugin(
        const nx::sdk::Ptr<const nx::sdk::IRefCountable>& pluginRefCountable) const
{
    std::vector<nx::vms::api::PluginResourceBindingInfo> result;
    for (const auto& bindingInfoHolder: m_proxiedBindingInfoHolders)
    {
        const std::vector<nx::vms::api::PluginResourceBindingInfo> bindingInfo =
            bindingInfoHolder->bindingInfoForPlugin(pluginRefCountable);

        result.insert(result.end(), bindingInfo.begin(), bindingInfo.end());
    }

    return result;
}
