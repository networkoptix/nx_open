#include "server_plugin_data_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/media_server_resource.h>

#include <nx/vms/api/data/analytics_data.h>

#include "../redux/server_settings_dialog_store.h"

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// ServerPluginDataWatcher::Private

class ServerPluginDataWatcher::Private: public QObject
{
    ServerPluginDataWatcher* const q;

public:
    explicit Private(ServerPluginDataWatcher* q, ServerSettingsDialogStore* store) :
        q(q),
        m_store(store)
    {
    }

    QnMediaServerResourcePtr server() const
    {
        return m_server;
    }

    void setServer(const QnMediaServerResourcePtr& value)
    {
        if (m_server == value)
            return;

        if (m_server)
            m_server->disconnect(this);

        m_server = value;

        // TODO: #vkutin Request data from the server if it's online.
        // Watch online/offline server transitions.

        // This is a debug stub:
        if (m_store)
        {
            const auto makePluginData =
                [](QString name, QString desc, QString libraryName, QString vendor, QString ver)
                {
                    nx::vms::api::PluginModuleData data;
                    data.name = name;
                    data.description = desc;
                    data.libraryName = libraryName;
                    data.vendor = vendor;
                    data.version = ver;
                    return data;
                };

            m_store->setPluginModules(nx::vms::api::PluginModuleDataList{
                makePluginData(
                    "Plugin 1",
                    "This is test plugin 1 description",
                    "plugin_1.dll",
                    "Vendor 1",
                    "1.0.1"),
                makePluginData(
                    "Plugin 2",
                    "This is test plugin 2 description",
                    "plugin_2.dll",
                    "Vendor 2",
                    "1.0.2"),
                makePluginData(
                    "Plugin 3",
                    "This is test plugin 3 description",
                    "plugin_3.dll",
                    "Vendor 3",
                    "3.0")});
        }
    }

private:
    QPointer<ServerSettingsDialogStore> m_store;
    QnMediaServerResourcePtr m_server;
};

// ------------------------------------------------------------------------------------------------
// ServerPluginDataWatcher

ServerPluginDataWatcher::ServerPluginDataWatcher(ServerSettingsDialogStore* store, QObject* parent) :
    base_type(parent),
    d(new Private(this, store))
{
}

ServerPluginDataWatcher::~ServerPluginDataWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnMediaServerResourcePtr ServerPluginDataWatcher::server() const
{
    return d->server();
}

void ServerPluginDataWatcher::setServer(const QnMediaServerResourcePtr& value)
{
    d->setServer(value);
}

} // nx::vms::client::desktop
