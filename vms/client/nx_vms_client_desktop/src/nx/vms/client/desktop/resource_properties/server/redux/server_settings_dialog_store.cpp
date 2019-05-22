#include "server_settings_dialog_store.h"

#include <type_traits>

#include <QtCore/QAbstractListModel>
#include <QtCore/QScopedValueRollback>

#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/desktop/common/redux/private_redux_store.h>

#include "server_settings_dialog_state.h"
#include "server_settings_dialog_state_reducer.h"

namespace nx::vms::client::desktop {

using State = ServerSettingsDialogState;
using Reducer = ServerSettingsDialogStateReducer;

namespace {

QVariantMap pluginData(const nx::vms::api::PluginModuleData& plugin)
{
    return QVariantMap({
        {"name", plugin.name},
        {"description", plugin.description},
        {"libraryName", plugin.libraryName},
        {"vendor", plugin.vendor},
        {"version", plugin.version}});
}

QVariant pluginData(const std::optional<const nx::vms::api::PluginModuleData>& plugin)
{
    return plugin ? pluginData(*plugin) : QVariant();
}

QVariantList pluginDetails(const nx::vms::api::PluginModuleData& plugin)
{
    return QVariantList({
        QVariantMap({{"name", ServerSettingsDialogStore::tr("Library")}, {"value", plugin.libraryName}}),
        QVariantMap({{"name", ServerSettingsDialogStore::tr("Version")}, {"value", plugin.version}}),
        QVariantMap({{"name", ServerSettingsDialogStore::tr("Vendor")}, {"value", plugin.vendor}}) });
}

QVariantList pluginDetails(const std::optional<const nx::vms::api::PluginModuleData>& plugin)
{
    return plugin ? pluginDetails(*plugin) : QVariantList();
}

} // namespace

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

bool ServerSettingsDialogStore::isOnline() const
{
    return d->state.isOnline;
}

void ServerSettingsDialogStore::setOnline(bool value)
{
    if (d->state.isOnline == value)
        return;

    d->executeAction([&](State state) { return Reducer::setOnline(std::move(state), value); });
}

QVariantList ServerSettingsDialogStore::pluginModules() const
{
    QVariantList result;
    for (const auto& plugin: d->state.plugins.modules)
        result.push_back(pluginData(plugin));

    return result;
}

void ServerSettingsDialogStore::setPluginModules(const nx::vms::api::PluginModuleDataList& value)
{
    d->executeAction(
        [&](State state) { return Reducer::setPluginModules(std::move(state), value); });
}

QVariant ServerSettingsDialogStore::currentPlugin() const
{
    return pluginData(d->state.currentPlugin());
}

void ServerSettingsDialogStore::selectCurrentPlugin(const QString& libraryName)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::selectCurrentPlugin(std::move(state), libraryName.trimmed());
        });
}

QVariantList ServerSettingsDialogStore::currentPluginDetails() const
{
    return pluginDetails(d->state.currentPlugin());
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
