#pragma once

#include <vector>

#include <nx/vms/server/metrics/plugin_resource_binding_info_provider.h>

class ProxyPluginBindingInfoHolder: public nx::vms::server::metrics::PluginResourceBindingInfoHolder
{
public:
    using BindingInfoHolderList =
        std::vector<std::unique_ptr<nx::vms::server::metrics::PluginResourceBindingInfoHolder>>;
public:
    ProxyPluginBindingInfoHolder(BindingInfoHolderList bindingInfoHolderList);

    virtual std::vector<nx::vms::api::PluginResourceBindingInfo> bindingInfoForPlugin(
        const nx::sdk::Ptr<const nx::sdk::IRefCountable>& pluginRefCountable) const override;

private:
    BindingInfoHolderList m_proxiedBindingInfoHolders;

};
