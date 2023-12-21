// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

/**
 * Qml version of QSortFilterProxyModel. Note that if the source model is empty and has no roles
 * (like ListModel), the roles will be determined during the first insertion.
 *
 * Example:
 * <pre><code>
 *     SortFilterProxyModel
 *     {
 *         sourceModel: model
 *         sortRoleName: "name"
 *         filterRoleName: "name"
 *         filterRegularExpression: new RegExp(search.text, "i")
 *     }
 * </code></pre>
 */
class SortFilterProxyModel: public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString sortRoleName
        READ sortRoleName WRITE setSortRoleName NOTIFY sortRoleNameChanged)
    Q_PROPERTY(QString filterRoleName
        READ filterRoleName WRITE setFilterRoleName NOTIFY filterRoleNameChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY sortColumnChanged)
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder NOTIFY sortOrderChanged)

public:
    SortFilterProxyModel(QObject* parent = nullptr);

    void setSortRoleName(const QString& name, bool force = false);
    QString sortRoleName() const;
    void setFilterRoleName(const QString& name, bool force = false);
    QString filterRoleName() const;

    int count() const;
    int sourceRole(const QString& name) const;

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    static void registerQmlType();

signals:
    void countChanged();
    void sortRoleNameChanged();
    void filterRoleNameChanged();
    void sortColumnChanged();
    void sortOrderChanged();

private:
    nx::utils::ScopedConnection m_rowsInsertedConnection;
    QString m_sortRoleName;
    QString m_filterRoleName;
};

} // namespace nx::vms::client::desktop
