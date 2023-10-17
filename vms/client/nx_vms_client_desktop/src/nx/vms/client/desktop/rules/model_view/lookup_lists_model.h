// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::rules {

class LookupListsModel: public QAbstractListModel, public SystemContextAware
{
    Q_OBJECT

public:
    enum Roles
    {
        LookupListIdRole = Qt::UserRole,
    };
    Q_ENUM(Roles)

    LookupListsModel(SystemContext* context, QObject* parent);

    std::optional<QString> objectTypeId() const;
    void setObjectTypeId(const QString& type);

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private:
    std::optional<QString> m_objectTypeId;
    std::vector<std::pair</*list id*/ QnUuid, /*list name*/ QString>> m_lookupLists;
};

} // namespace nx::vms::client::desktop::rules
