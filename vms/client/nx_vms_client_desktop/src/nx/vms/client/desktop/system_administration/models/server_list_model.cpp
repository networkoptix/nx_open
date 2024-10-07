// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_list_model.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/manual_device_addition/dialog/private/current_system_servers.h>

namespace nx::vms::client::desktop {

struct ServerListModel::Private
{
    ServerListModel* q;
    std::unique_ptr<CurrentSystemServers> servers;
    std::unordered_map<nx::Uuid, nx::utils::ScopedConnections> connections;
    QList<nx::Uuid> serverIds;

    Private(ServerListModel* parent):
        q(parent)
    {
        clear();
    }

    void clear()
    {
        serverIds.clear();
        serverIds.append(nx::Uuid());
    }
};

ServerListModel::ServerListModel():
    SystemContextAware(SystemContext::fromQmlContext(this)), d(new Private(this))
{
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
    if (index.row() < 0 || index.row() >= d->serverIds.count())
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

    const auto serverId = d->serverIds[index.row()];
    const auto server = resourcePool()->getResourceById(serverId);

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
    return d->serverIds.indexOf(serverId);
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
    return d->serverIds.count();
}

void ServerListModel::classBegin()
{
    d->servers.reset(new CurrentSystemServers(systemContext()));
    d->clear();
    for (const auto& server: d->servers->servers())
        d->serverIds.append(server->getId());

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
            beginInsertRows({}, d->serverIds.count(), d->serverIds.count());
            d->serverIds.append(serverId);
            endInsertRows();
        });

    connect(d->servers.get(), &CurrentSystemServers::serverRemoved,
        [this](const nx::Uuid& serverId)
        {
            d->connections.erase(serverId);
            const int serverIndex = d->serverIds.indexOf(serverId);
            if (serverIndex <= 0)
                return;

            beginRemoveRows({}, serverIndex, serverIndex);
            d->serverIds.removeAt(serverIndex);
            endRemoveRows();
            emit serverRemoved();
        });
}

} // namespace nx::vms::client::desktop
