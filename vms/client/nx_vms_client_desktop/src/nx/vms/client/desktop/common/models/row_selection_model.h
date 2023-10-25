// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QAbstractProxyModel>

#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API RowSelectionModel: public QAbstractProxyModel
{
    Q_OBJECT
    using base_type = QAbstractProxyModel;

public:
    static void registerQmlType();

public:
    explicit RowSelectionModel(QObject* parent = nullptr);
    virtual ~RowSelectionModel() override;

    Q_INVOKABLE QVector<int> getSelectedRows() const;
    Q_INVOKABLE void setCheckboxColumnVisible(const bool visible);

    virtual void setSourceModel(QAbstractItemModel* sourceModel) override;

    virtual QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

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

private:
    bool m_checkboxColumnVisible = true;
    std::set<QPersistentModelIndex> m_checkedRows;
    nx::utils::ScopedConnections m_connections;

    QList<QPersistentModelIndex> m_layoutChangePersistentIndexes;
    QModelIndexList m_proxyIndexes;
};

} // namespace nx::vms::client::desktop
