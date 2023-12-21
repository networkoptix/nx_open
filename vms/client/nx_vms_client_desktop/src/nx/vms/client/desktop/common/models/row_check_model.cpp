// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "row_check_model.h"

#include <QtQml/QQmlEngine>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

void RowCheckModel::registerQmlType()
{
    qmlRegisterType<RowCheckModel>("nx.vms.client.desktop", 1, 0, "RowCheckModel");
}

RowCheckModel::RowCheckModel(QObject* parent):
    base_type(parent)
{
}

RowCheckModel::~RowCheckModel()
{
}

QList<int> RowCheckModel::checkedRows() const
{
    QList<int> result; // TODO: #mmalofeev Consider use list as index holder.
    for (const auto& i: m_checkedRows)
    {
        if (i.isValid())
            result.push_back(i.row());
    }

    std::sort(result.begin(), result.end());
    return result;
}

void RowCheckModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    m_checkedRows.clear();

    base_type::setSourceModel(sourceModel);

    emit checkedRowsChanged();

    m_connections.reset();

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset, this,
            [this]
            {
                beginResetModel();
                m_checkedRows.clear();
                emit checkedRowsChanged();
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::modelReset,
            this, &RowCheckModel::endResetModel);

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this,
            [this](const QModelIndex& parent, int first, int last)
            {
                beginInsertRows(mapFromSource(parent), first, last);
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsInserted,
            this, &RowCheckModel::endInsertRows);

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex& parent, int first, int last)
            {
                beginRemoveRows(mapFromSource(parent), first, last);
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsRemoved,this,
            [this]()
            {
                endRemoveRows();
                emit checkedRowsChanged();
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsAboutToBeMoved, this,
            [this](const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
                const QModelIndex& destinationParent, int destinationRow)
            {
                NX_ASSERT(!sourceParent.isValid() && !destinationParent.isValid());

                if (sourceFirst <= destinationRow && destinationRow <= sourceLast)
                    return;

                beginMoveRows(
                    mapFromSource(sourceParent),
                    sourceFirst,
                    sourceLast,
                    mapFromSource(destinationParent),
                    destinationRow);
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsMoved,
            this, &RowCheckModel::endMoveRows);

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::layoutAboutToBeChanged, this,
            [this](
                const QList<QPersistentModelIndex>& sourceParents,
                QAbstractItemModel::LayoutChangeHint hint)
            {
                NX_ASSERT(sourceParents.isEmpty());

                emit layoutAboutToBeChanged({}, hint);

                const auto proxyPersistentIndexes = persistentIndexList();
                for (const QModelIndex& proxyPersistentIndex: proxyPersistentIndexes)
                {
                    m_proxyIndexes.append(proxyPersistentIndex);
                    NX_ASSERT(proxyPersistentIndex.isValid());
                    const QPersistentModelIndex srcPersistentIndex =
                        mapToSource(proxyPersistentIndex);
                    NX_ASSERT(srcPersistentIndex.isValid());
                    m_layoutChangePersistentIndexes.append(srcPersistentIndex);
                }
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::layoutChanged, this,
            [this](
                const QList<QPersistentModelIndex>& sourceParents,
                QAbstractItemModel::LayoutChangeHint hint)
            {
                NX_ASSERT(sourceParents.isEmpty());

                for (int i = 0; i < m_proxyIndexes.size(); ++i)
                {
                    QModelIndex targetIndex;
                    if (m_proxyIndexes.at(i).column() == 0)
                    {
                        // Restore target index from the nearest sibling index.
                        targetIndex =
                            index(mapFromSource(m_layoutChangePersistentIndexes.at(i)).row(), 0, {});
                    }
                    else
                    {
                        targetIndex = mapFromSource(m_layoutChangePersistentIndexes.at(i));
                    }

                    changePersistentIndex(m_proxyIndexes.at(i), targetIndex);
                }

                m_layoutChangePersistentIndexes.clear();
                m_proxyIndexes.clear();

                emit layoutChanged({}, hint);
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::dataChanged, this,
            [this](
                const QModelIndex& topLeft,
                const QModelIndex& bottomRight,
                const QList<int>& roles)
            {
                emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::headerDataChanged, this,
            [this](Qt::Orientation orientation, int first, int last)
            {
                emit headerDataChanged(orientation, first + 1, last + 1);
            });
}

QVariant RowCheckModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    NX_ASSERT(section < columnCount());

    if (orientation != Qt::Orientation::Horizontal)
        return QVariant();

    if (section == 0)
    {
        if (role == Qt::CheckStateRole)
            return Qt::Unchecked;

        return {};
    }

    return sourceModel()->headerData(section - 1, orientation, role);
}

QVariant RowCheckModel::data(const QModelIndex& index, int role) const
{
    NX_ASSERT(index.isValid());

    if (index.column() == 0 && role == Qt::CheckStateRole)
    {
        const QModelIndex checkboxIndex = sourceModel()->index(index.row(), 0);
        if (m_checkedRows.count(checkboxIndex) > 0)
            return Qt::Checked;

        return Qt::Unchecked;
    }

    return sourceModel()->data(mapToSource(index), role);
}

bool RowCheckModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    NX_ASSERT(index.isValid());

    if (index.column() == 0 && role == Qt::CheckStateRole)
    {
        const QModelIndex checkboxIndex = sourceModel()->index(index.row(), 0);
        if (m_checkedRows.count(checkboxIndex) > 0)
            m_checkedRows.erase(QPersistentModelIndex(checkboxIndex));
        else
            m_checkedRows.insert(QPersistentModelIndex(checkboxIndex));

        emit checkedRowsChanged();

        emit dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }

    return sourceModel()->setData(mapToSource(index), value, role);
}

QModelIndex RowCheckModel::index(int row, int column, const QModelIndex& /*parent*/) const
{
    return createIndex(row, column);
}

QModelIndex RowCheckModel::parent(const QModelIndex& /*index*/) const
{
    return {};
}

QModelIndex RowCheckModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid() || !sourceModel())
        return {};

    NX_ASSERT(sourceIndex.row() < sourceModel()->rowCount());

    return index(sourceIndex.row(), sourceIndex.column() + 1);
}

QModelIndex RowCheckModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid() || !sourceModel())
        return {};

    NX_ASSERT(proxyIndex.row() < rowCount());

    if (proxyIndex.column() == 0)
    {
        // Get the nearest sibling for the index from check box column.
        return sourceModel()->index(proxyIndex.row(), proxyIndex.column());
    }

    return sourceModel()->index(proxyIndex.row(), proxyIndex.column() - 1);
}

int RowCheckModel::rowCount(const QModelIndex& parent) const
{
    if (!sourceModel() || parent.isValid())
        return 0;

    return sourceModel()->rowCount();
}

int RowCheckModel::columnCount(const QModelIndex& parent) const
{
    if (!sourceModel() || parent.isValid())
        return 0;

    return sourceModel()->columnCount() + 1;
}

void RowCheckModel::sort(int column, Qt::SortOrder order)
{
    base_type::sort(column - 1, order);

    m_sortColumn = column;
    m_sortOrder = order;
    emit sortColumnChanged();
    emit sortOrderChanged();
}

QHash<int, QByteArray> RowCheckModel::roleNames() const
{
    auto result = base_type::roleNames();
    result[Qt::CheckStateRole] = "checkState";
    return result;
}

int RowCheckModel::sortColumn() const
{
    return m_sortColumn;
}

Qt::SortOrder RowCheckModel::sortOrder() const
{
    return m_sortOrder;
}

} // namespace nx::vms::client::desktop
