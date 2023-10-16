// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sort_filter_proxy_model.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {

SortFilterProxyModel::SortFilterProxyModel(QObject* parent):
    QSortFilterProxyModel(parent)
{
    connect(this, &QSortFilterProxyModel::sourceModelChanged, this,
        [this]
        {
            auto updateRoles =
                [this]
                {
                    setSortRoleName(m_sortRoleName, /*force*/ true);
                    setFilterRoleName(m_filterRoleName, /*force*/ true);
                };

            // Roles may be added during the first insertion.
            m_rowsInsertedConnection.reset(
                connect(sourceModel(), &QAbstractItemModel::rowsInserted, this,
                    [=]
                    {
                        updateRoles();
                        m_rowsInsertedConnection.reset();
                    }));

            updateRoles();
        });

    sort(/*column*/ 0); //< Start sorting.
}

int SortFilterProxyModel::sourceRole(const QString& name) const
{
    return roleNames().key(name.toUtf8(), Qt::DisplayRole);
}

void SortFilterProxyModel::setSortRoleName(const QString& name, bool force)
{
    if (m_sortRoleName == name && !force)
        return;

    m_sortRoleName = name;
    emit sortRoleNameChanged();
    setSortRole(sourceRole(m_sortRoleName));
}

QString SortFilterProxyModel::sortRoleName() const
{
    return m_sortRoleName;
}

void SortFilterProxyModel::setFilterRoleName(const QString& name, bool force)
{
    if (m_filterRoleName == name && !force)
        return;

    m_filterRoleName = name;
    emit filterRoleNameChanged();
    setFilterRole(sourceRole(m_filterRoleName));
}

QString SortFilterProxyModel::filterRoleName() const
{
    return m_filterRoleName;
}

void SortFilterProxyModel::registerQmlType()
{
    qmlRegisterType<SortFilterProxyModel>("nx.vms.client.desktop", 1, 0, "SortFilterProxyModel");
}

} // namespace nx::vms::client::desktop
