// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_plugin_data_watcher.h"

#include <QtCore/QPointer>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

#include "../flux/server_settings_dialog_store.h"

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// ServerPluginDataWatcher::Private

class ServerPluginDataWatcher::Private:
    public QObject,
    public core::RemoteConnectionAware
{
public:
    explicit Private(ServerPluginDataWatcher* /*q*/, ServerSettingsDialogStore* store) :
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
        clearActiveRequest();
        if (!m_store)
            return;

        const bool isOnline = m_server && m_server->isOnline();
        m_store->setOnline(isOnline);
        if (isOnline)
            requestData();
    }

    void requestData()
    {
        if (!NX_ASSERT(m_server) || !NX_ASSERT(connection()))
            return;

        const auto handleResponse = nx::utils::guarded(this,
            [this](bool success, rest::Handle requestId, nx::network::rest::JsonResult result)
            {
                if (success && requestId == m_requestId && m_store)
                    m_store->setPluginModules(result.deserialized<nx::vms::api::PluginInfoList>());

                clearActiveRequest();
            });

        m_requestId = connectedServerApi()->getPluginInformation(
            m_server->getId(),
            handleResponse,
            thread());
        if (m_store)
            m_store->setPluginsInformationLoading(m_requestId != rest::Handle{});
    }

    void clearActiveRequest()
    {
        m_requestId = {};
        if (m_store)
            m_store->setPluginsInformationLoading(false);
    }

private:
    QPointer<ServerSettingsDialogStore> m_store;
    QnMediaServerResourcePtr m_server;
    rest::Handle m_requestId{};
};

// ------------------------------------------------------------------------------------------------
// ServerPluginDataWatcher

ServerPluginDataWatcher::ServerPluginDataWatcher(ServerSettingsDialogStore* store, QObject* parent):
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
