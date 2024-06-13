// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_access_model.h"

#include <QtQml/QtQml>

#include <nx/vms/client/desktop/resource/server.h>

namespace nx::vms::client::desktop {

struct RemoteAccessModel::Private
{
    RemoteAccessData remoteAccessData;
    ServerResourcePtr server;
};

RemoteAccessModel::RemoteAccessModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

RemoteAccessModel::~RemoteAccessModel()
{
}

void RemoteAccessModel::setServer(const ServerResourcePtr& server)
{
    if (server == d->server)
        return;

    if (server)
        server->disconnect(this);

    d->server = server;

    connect(server.get(),
        &ServerResource::remoteAccessDataChanged,
        this,
        &RemoteAccessModel::updateModel);

    updateModel();
}

QHash<int, QByteArray> RemoteAccessModel::roleNames() const
{
    static QHash<int, QByteArray> kRoleNames{
        {static_cast<int>(Roles::name), "name"},
        {static_cast<int>(Roles::login), "login"},
        {static_cast<int>(Roles::password), "password"},
        {static_cast<int>(Roles::port), "port"},
    };

    return kRoleNames;
}

QVariant RemoteAccessModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const auto& item = d->remoteAccessData.forwardedPortConfigurations[index.row()];

    switch (static_cast<Roles>(role))
    {
        case Roles::name:
            return QVariant::fromValue(item.name);
        case Roles::login:
            return QVariant::fromValue(item.login);
        case Roles::password:
            return QVariant::fromValue(item.password);
        case Roles::port:
            return item.forwardedPort != 0 ? QVariant::fromValue(item.forwardedPort) : QVariant();
    }

    return QVariant();
}

int RemoteAccessModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return d->remoteAccessData.forwardedPortConfigurations.size();
}

bool RemoteAccessModel::enabledForCurrentUser() const
{
    return d->remoteAccessData.enabledForCurrentUser;
}

void RemoteAccessModel::updateModel()
{
    beginResetModel();
    d->remoteAccessData = d->server->remoteAccessData();
    endResetModel();
    emit enabledForCurrentUserChanged();
}

} // namespace nx::vms::client::desktop
