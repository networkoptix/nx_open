#include "utility_provider.h"

#include <nx/sdk/uuid.h>
#include <plugins/plugin_container_api.h> //< for IID_TimeProvider
#include <utils/common/synctime.h>
#include <nx/sdk/helpers/string.h>
#include <plugins/plugin_manager.h>

namespace nx::vms::server::plugins {

nx::sdk::IRefCountable* UtilityProvider::queryInterface(InterfaceId id)
{
    // TODO: When nxpl::TimeProvider is deleted, move the value of its IID here to preserve binary
    // compatibility with old SDK plugins.
    return queryInterfaceSupportingDeprecatedId(id, nx::sdk::Uuid(nxpl::IID_TimeProvider.bytes));
}

int64_t UtilityProvider::vmsSystemTimeSinceEpochMs() const
{
    return qnSyncTime->currentMSecsSinceEpoch();
}

const nx::sdk::IString* UtilityProvider::homeDir(const nx::sdk::IPlugin* plugin) const
{
    if (!plugin)
    {
        NX_ERROR(this, "Plugin called homeDir(nullptr)");
        return new nx::sdk::String("");
    }

    if (const auto pluginInfo = m_pluginManager->pluginInfo(plugin))
        return new nx::sdk::String(pluginInfo->homeDir.toStdString());

    return new nx::sdk::String(""); //< The error is already logged.
}

} // namespace nx::vms::server::plugins
