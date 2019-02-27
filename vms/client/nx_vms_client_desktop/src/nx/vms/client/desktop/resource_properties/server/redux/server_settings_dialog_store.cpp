#include "server_settings_dialog_store.h"

#include <type_traits>

#include <QtCore/QScopedValueRollback>

#include <nx/vms/client/desktop/common/redux/private_redux_store.h>

#include "server_settings_dialog_state.h"
#include "server_settings_dialog_state_reducer.h"

namespace nx::vms::client::desktop {

using State = ServerSettingsDialogState;
using Reducer = ServerSettingsDialogStateReducer;

struct ServerSettingsDialogStore::Private:
    PrivateReduxStore<ServerSettingsDialogStore, State>
{
    using PrivateReduxStore::PrivateReduxStore;
};

ServerSettingsDialogStore::ServerSettingsDialogStore(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

ServerSettingsDialogStore::~ServerSettingsDialogStore()
{
}

const State& ServerSettingsDialogStore::state() const
{
    return d->state;
}

void ServerSettingsDialogStore::loadServer(const QnMediaServerResourcePtr& server)
{
    d->executeAction([&](State state) { return Reducer::loadServer(std::move(state), server); });
}

QVariantList ServerSettingsDialogStore::pluginModules() const
{
    QVariantList result;
    for (const auto& plugin: d->state.plugins.modules)
    {
        // TODO: #vkutin Automate this?
        result.append(QVariantMap{
            {"name", plugin.name},
            {"description", plugin.description},
            {"libraryName", plugin.libraryName},
            {"vendor", plugin.vendor},
            {"version", plugin.version}});
    }
    return result;
}

void ServerSettingsDialogStore::setPluginModules(const nx::vms::api::PluginModuleDataList& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setPluginModules(std::move(state), value); });
}

QString ServerSettingsDialogStore::currentPluginLibraryName() const
{
    return d->state.plugins.currentLibraryName;
}

void ServerSettingsDialogStore::setCurrentPluginLibraryName(const QString& value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setCurrentPluginLibraryName(std::move(state), value);
        });
}

bool ServerSettingsDialogStore::pluginsInformationLoading() const
{
    return d->state.plugins.loading;
}

void ServerSettingsDialogStore::setPluginsInformationLoading(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setPluginsInformationLoading(std::move(state), value);
        });
}

} // namespace nx::vms::client::desktop
