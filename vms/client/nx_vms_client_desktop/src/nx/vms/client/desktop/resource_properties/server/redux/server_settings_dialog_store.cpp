#include "server_settings_dialog_store.h"

#include <type_traits>

#include <QtCore/QAbstractListModel>
#include <QtCore/QFileInfo>
#include <QtCore/QScopedValueRollback>

#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/desktop/common/redux/private_redux_store.h>

#include "server_settings_dialog_state.h"
#include "server_settings_dialog_state_reducer.h"

namespace nx::vms::client::desktop {

using State = ServerSettingsDialogState;
using Reducer = ServerSettingsDialogStateReducer;

namespace {

using PluginInfo = nx::vms::api::PluginInfo;

QVariantMap pluginData(const PluginInfo& plugin)
{
    const auto pluginName = plugin.name.isEmpty()
        ? QFileInfo(plugin.libraryName).fileName()
        : plugin.name;

    return QVariantMap({
        {"id", plugin.libraryName},
        {"name", pluginName},
        {"description", plugin.description},
        {"loaded", plugin.status == PluginInfo::Status::loaded}});
}

QVariant pluginData(const std::optional<const PluginInfo>& plugin)
{
    return plugin ? pluginData(*plugin) : QVariant();
}

QString pluginError(PluginInfo::Error errorCode)
{
    switch (errorCode)
    {
        case PluginInfo::Error::cannotLoadLibrary:
            return ServerSettingsDialogStore::tr("library file cannot be loaded");

        case PluginInfo::Error::invalidLibrary:
            return ServerSettingsDialogStore::tr("invalid or incompatible plugin library");

        case PluginInfo::Error::libraryFailure:
            return ServerSettingsDialogStore::tr("plugin library failed to initialize");

        case PluginInfo::Error::unsupportedVersion:
            return ServerSettingsDialogStore::tr("plugin API version is no longer supported");

        default:
            return ServerSettingsDialogStore::tr("unknown error");
    }
}

QString pluginStatus(const PluginInfo& plugin)
{
    const auto notLoadedMessage =
        [](const QString& details)
        {
            return QString("%1: %2").arg(ServerSettingsDialogStore::tr("Not loaded"), details);
        };

    switch (plugin.status)
    {
        case PluginInfo::Status::loaded:
            return ServerSettingsDialogStore::tr("Loaded");

        case PluginInfo::Status::notLoadedBecauseOfError:
            return notLoadedMessage(pluginError(plugin.errorCode));

        case PluginInfo::Status::notLoadedBecauseOfBlackList:
            return notLoadedMessage(ServerSettingsDialogStore::tr("plugin is in the black list"));

        case PluginInfo::Status::notLoadedBecauseOptional:
            return notLoadedMessage(ServerSettingsDialogStore::tr(
                "plugin is optional and isn't in the white list"));

        default:
            NX_ASSERT(false);
            return "<unknown status>";
    }
}

QVariantList pluginDetails(const PluginInfo& plugin)
{
    QVariantList result;

    const auto add =
        [&](const QString& name, const QString& value)
        {
            result << QVariantMap({{"name", name}, {"value", value}});
        };

    const auto addNonEmpty =
        [&](const QString& name, const QString& value)
        {
            if (!value.isEmpty())
                add(name, value);
        };

    add(ServerSettingsDialogStore::tr("Library"), plugin.libraryName);
    addNonEmpty(ServerSettingsDialogStore::tr("Version"), plugin.version.trimmed());
    addNonEmpty(ServerSettingsDialogStore::tr("Vendor"), plugin.vendor.trimmed());
    add(ServerSettingsDialogStore::tr("Status"), pluginStatus(plugin));

    return result;
}

QVariantList pluginDetails(const std::optional<const PluginInfo>& plugin)
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

void ServerSettingsDialogStore::setPluginModules(const nx::vms::api::PluginInfoList& value)
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
