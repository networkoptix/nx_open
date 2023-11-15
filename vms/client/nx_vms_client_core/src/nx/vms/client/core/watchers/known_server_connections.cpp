// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "known_server_connections.h"

#include <chrono>

#include <QtCore/QTimer>

#include <client/client_message_processor.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx_ec/abstract_ec_connection.h>

namespace {

template<typename T>
bool trimConnectionsList(QList<T>& list)
{
    const int kMaxItemsToStore = nx::vms::client::core::ini().maxLastConnectedTilesStored;

    if (list.size() <= kMaxItemsToStore)
        return false;

    list.erase(list.begin() + kMaxItemsToStore, list.end());
    return true;
}

} // namespace

namespace nx::vms::client::core {
namespace watchers {

class KnownServerConnections::Private:
    public QObject,
    public QnCommonModuleAware,
    public RemoteConnectionAware
{
public:
    Private(QnCommonModule* commonModule);

    void start();

private:
    void saveConnection(const QnUuid& serverId, nx::network::SocketAddress address);

private:
    nx::vms::discovery::Manager* m_discoveryManager = nullptr;
    QList<KnownServerConnections::Connection> m_connections;
};

KnownServerConnections::Private::Private(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_discoveryManager(appContext()->moduleDiscoveryManager()) //< Can be absent in unit tests.
{
}

void KnownServerConnections::Private::start()
{
    m_connections = appContext()->coreSettings()->knownServerConnections();
    const int oldSize = m_connections.size();
    trimConnectionsList(m_connections);

    if (m_connections.size() != oldSize)
        appContext()->coreSettings()->knownServerConnections = m_connections;

    if (m_discoveryManager)
    {
        for (const auto& connection: m_connections)
            m_discoveryManager->checkEndpoint(connection.url, connection.serverId);
    }

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened, this,
        [this]()
        {
            if (!connection()->connectionInfo().isTemporary())
                saveConnection(connection()->moduleInformation().id, connectionAddress());
        });
}

void KnownServerConnections::Private::saveConnection(
    const QnUuid& serverId,
    nx::network::SocketAddress address)
{
    NX_ASSERT(!address.isNull());
    if (nx::network::SocketGlobals::addressResolver().isCloudHostname(address.address.toString()))
        return;

    Connection connection{
        serverId,
        nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(address)
            .toUrl()};

    NX_VERBOSE(this, "Saving connection, id=%1 url=%2", connection.serverId, connection.url);

    // Place url to the top of the list.
    m_connections.removeOne(connection);
    m_connections.prepend(connection);
    trimConnectionsList(m_connections);

    if (m_discoveryManager)
        m_discoveryManager->checkEndpoint(connection.url, connection.serverId);
    appContext()->coreSettings()->knownServerConnections = m_connections;
}

KnownServerConnections::KnownServerConnections(QnCommonModule* commonModule, QObject* parent):
    QObject(parent),
    d(new Private(commonModule))
{
}

KnownServerConnections::~KnownServerConnections()
{
}

void KnownServerConnections::start()
{
    d->start();
}

} // namespace watchers
} // namespace nx::vms::client::core
