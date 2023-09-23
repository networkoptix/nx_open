// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "known_server_connections.h"

#include <chrono>

#include <QtCore/QTimer>

#include <nx/network/address_resolver.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx_ec/abstract_ec_connection.h>

namespace nx::vms::client::core {
namespace watchers {

namespace {

bool trimConnectionsList(QList<KnownServerConnections::Connection>& list)
{
    const int kMaxItemsToStore = nx::vms::client::core::ini().maxLastConnectedTilesStored;

    if (list.size() <= kMaxItemsToStore)
        return false;

    list.erase(list.begin() + kMaxItemsToStore, list.end());
    return true;
}

} // namespace

struct KnownServerConnections::Private
{
    void start();

    nx::vms::discovery::Manager* discoveryManager = appContext()->moduleDiscoveryManager();
    QList<Connection> connections;
};

void KnownServerConnections::Private::start()
{
    connections = appContext()->coreSettings()->knownServerConnections();
    const int oldSize = connections.size();
    trimConnectionsList(connections);

    if (connections.size() != oldSize)
        appContext()->coreSettings()->knownServerConnections = connections;

    if (discoveryManager)
    {
        for (const auto& connection: connections)
            discoveryManager->checkEndpoint(connection.url, connection.serverId);
    }
}

KnownServerConnections::KnownServerConnections(QObject* parent):
    QObject(parent),
    d(new Private())
{
}

KnownServerConnections::~KnownServerConnections()
{
}

void KnownServerConnections::start()
{
    d->start();
}

void KnownServerConnections::saveConnection(
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
    d->connections.removeOne(connection);
    d->connections.prepend(connection);
    trimConnectionsList(d->connections);

    if (d->discoveryManager)
        d->discoveryManager->checkEndpoint(connection.url, connection.serverId);
    appContext()->coreSettings()->knownServerConnections = d->connections;
}

} // namespace watchers
} // namespace nx::vms::client::core
