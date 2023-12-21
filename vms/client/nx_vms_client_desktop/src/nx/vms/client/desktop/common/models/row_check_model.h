// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QAbstractProxyModel>

#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

// TODO: #mmalofeev consider inherit from the SortFilterProxyModel.
class NX_VMS_CLIENT_DESKTOP_API RowCheckModel: public QAbstractProxyModel
{
    Q_OBJECT
    using base_type = QAbstractProxyModel;

    Q_PROPERTY(QList<int> checkedRows READ checkedRows NOTIFY checkedRowsChanged)
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY sortColumnChanged)
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder NOTIFY sortOrderChanged)

public:
    explicit RowCheckModel(QObject* parent = nullptr);
    virtual ~RowCheckModel() override;

    QList<int> checkedRows() const;

    virtual void setSourceModel(QAbstractItemModel* sourceModel) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole) override;

    virtual QModelIndex	index(
        int row,
        int column,
        const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex	parent(const QModelIndex& index) const override;

    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;

    virtual int	rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int	columnCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    virtual QHash<int, QByteArray> roleNames() const override;

    int sortColumn() const;
    Qt::SortOrder sortOrder() const;

    static void registerQmlType();

signals:
    void checkedRowsChanged();
    void sortColumnChanged();
    void sortOrderChanged();

private:
    std::set<QPersistentModelIndex> m_checkedRows;
    nx::utils::ScopedConnections m_connections;

    QList<QPersistentModelIndex> m_layoutChangePersistentIndexes;
    QModelIndexList m_proxyIndexes;

    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder{};
};

} // namespace nx::vms::client::desktop
