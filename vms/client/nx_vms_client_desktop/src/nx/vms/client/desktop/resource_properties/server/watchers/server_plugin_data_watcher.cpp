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
        handleStateChange();

        if (m_server)
            connect(m_server.get(), &QnResource::statusChanged, this, &Private::handleStateChange);
    }

private:
    void handleStateChange()
    {
        if (!m_store)
            return;

        cancelActiveRequest();

        const bool isOnline = m_server && m_server->isOnline();
        m_store->setOnline(isOnline);
        if (isOnline)
            requestData();
    }

    // TODO: FIXME: #vkutin The following 3 methods are a debug stub.
    // Replace them with actual request when it's implemented.

    void requestData()
    {
        m_requestTimerDEBUG = new QTimer(this);
        m_requestTimerDEBUG->setSingleShot(true);
        m_requestTimerDEBUG->start(3000);

        connect(m_requestTimerDEBUG.data(), &QTimer::timeout, this,
            [this]()
            {
                m_requestTimerDEBUG->deleteLater();
                handleDataReceived();
            });

        if (m_store)
            m_store->setPluginsInformationLoading(true);
    }

    void handleDataReceived()
    {
        if (!m_store)
            return;

        const auto makePluginData =
            [](QString name, QString desc, QString libraryName, QString vendor, QString ver)
            {
                nx::vms::api::PluginInfo data;
                data.name = name;
                data.description = desc;
                data.libraryName = libraryName;
                data.vendor = vendor;
                data.version = ver;
                return data;
            };

        m_store->setPluginModules(nx::vms::api::PluginInfoList{
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

        m_store->setPluginsInformationLoading(false);
    }

    void cancelActiveRequest()
    {
        delete m_requestTimerDEBUG;
    }

private:
    QPointer<ServerSettingsDialogStore> m_store;
    QnMediaServerResourcePtr m_server;

    // TODO: FIXME: #vkutin As a debug stub here is a timer instead of a request handle.
    QPointer<QTimer> m_requestTimerDEBUG;
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
