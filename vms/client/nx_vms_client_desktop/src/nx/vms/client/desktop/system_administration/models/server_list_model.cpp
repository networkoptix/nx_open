// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_list_model.h"

#include <core/resource/media_server_resource.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/manual_device_addition/dialog/private/current_system_servers.h>

namespace nx::vms::client::desktop {

struct ServerListModel::Private
{
    ServerListModel* q;
    std::unique_ptr<CurrentSystemServers> servers;
    std::unordered_map<nx::Uuid, nx::utils::ScopedConnections> connections;

    Private(ServerListModel* parent):
        q(parent),
        servers(new CurrentSystemServers(q->systemContext()))
    {
    }
};

ServerListModel::ServerListModel():
    SystemContextAware(SystemContext::fromQmlContext(this)), d(new Private(this))
{
    connect(d->servers.get(), &CurrentSystemServers::serverAdded,
        [this](const QnMediaServerResourcePtr& server)
        {
            const auto serverId = server->getId();

            nx::utils::ScopedConnections serverConnections;

            serverConnections << connect(server.get(), &QnMediaServerResource::statusChanged,
                [this, serverId]
                {
                    const int row = indexOf(serverId);
                    if (row != 0)
                        emit dataChanged(index(row), index(row));
                });

            serverConnections << connect(server.get(), &QnMediaServerResource::nameChanged,
                [this, serverId]
                {
                    const int row = indexOf(serverId);
                    if (row != 0)
                        emit dataChanged(index(row), index(row));
                });

            d->connections[serverId] = std::move(serverConnections);

            beginInsertRows({}, 0, d->servers->serversCount());
            endInsertRows();
            emit dataChanged(index(0), index(d->servers->serversCount()));
        });

    connect(d->servers.get(), &CurrentSystemServers::serverRemoved,
        [this](const nx::Uuid& serverId)
        {
            d->connections.erase(serverId);
            beginRemoveRows({}, 0, d->servers->serversCount() + 1);
            endRemoveRows();
            emit dataChanged(index(0), index(d->servers->serversCount()));
            emit serverRemoved();
        });

    beginInsertRows({}, 0, d->servers->serversCount());
    endInsertRows();
}

ServerListModel::~ServerListModel()
{}

void ServerListModel::registerQmlType()
{
    qmlRegisterType<ServerListModel>("nx.vms.client.desktop", 1, 0, "ServerListModel");
}

QHash<int, QByteArray> ServerListModel::roleNames() const
{
    auto names = base_type::roleNames();
    names[idRole] = "serverId";
    names[onlineRole] = "isOnline";
    return names;
}

QVariant ServerListModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() > d->servers->serversCount())
        return {};

    if (index.row() == 0)
    {
        switch (role)
        {
            case Qt::DisplayRole:
                return tr("Auto");

            case idRole:
                return QVariant::fromValue(nx::Uuid{});

            case onlineRole:
                return true;

            default:
                return {};
        }
    }

    const auto server = d->servers->servers()[index.row() - 1];

    switch (role)
    {
        case Qt::DisplayRole:
            return server->getName();

        case idRole:
            return QVariant::fromValue(server->getId());

        case onlineRole:
            return server->getStatus() == nx::vms::api::ResourceStatus::online;

        default:
            break;
    }

    return {};
}

int ServerListModel::indexOf(const nx::Uuid& serverId)
{
    const auto servers = d->servers->servers();
    for (int i = 0; i < servers.size(); ++i)
    {
        if (servers.at(i)->getId() == serverId)
            return i + 1;
    }

    return 0;
}

nx::Uuid ServerListModel::serverIdAt(int row) const
{
    return data(index(row), idRole).value<nx::Uuid>();
}

QString ServerListModel::serverName(int row) const
{
    return data(index(row), Qt::DisplayRole).toString();
}

int ServerListModel::rowCount(const QModelIndex& /*parent*/) const
{
    return d->servers->serversCount() + 1;
}

} // namespace nx::vms::client::desktop
