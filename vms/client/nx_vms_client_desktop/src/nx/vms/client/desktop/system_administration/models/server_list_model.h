// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtQml/QQmlEngine>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

/**
 * A model for displaying a list of servers in the system for preffered LDAP sync server selection.
 * The first row of the model is a special value "Auto" with null server id.
 */
class ServerListModel: public QAbstractListModel, public SystemContextAware
{
    Q_OBJECT
    QML_ELEMENT

    using base_type = QAbstractListModel;

public:
    enum Roles
    {
        idRole = Qt::UserRole + 1,
        onlineRole,
    };

public:
    explicit ServerListModel();
    virtual ~ServerListModel() override;

    static void registerQmlType();

    virtual QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    Q_INVOKABLE nx::Uuid serverIdAt(int row) const;
    Q_INVOKABLE int indexOf(const nx::Uuid& serverId);
    Q_INVOKABLE QString serverName(int row) const;

signals:
    void serverRemoved();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
