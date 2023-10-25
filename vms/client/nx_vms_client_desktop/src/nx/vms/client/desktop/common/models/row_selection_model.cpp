// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "row_selection_model.h"

#include <QtQml/QQmlEngine>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

void RowSelectionModel::registerQmlType()
{
    qmlRegisterType<RowSelectionModel>("nx.vms.client.desktop", 1, 0, "RowSelectionModel");
}

RowSelectionModel::RowSelectionModel(QObject* parent):
    QAbstractProxyModel(parent)
{
}

RowSelectionModel::~RowSelectionModel()
{
}

QVector<int> RowSelectionModel::getSelectedRows() const
{
    QVector<int> result;

    for (auto iter = m_checkedRows.begin(); iter != m_checkedRows.end(); ++iter)
    {
        if (iter->isValid())
            result.push_back(iter->row());
    }

    std::sort(result.begin(), result.end());

    return result;
}

void RowSelectionModel::setCheckboxColumnVisible(const bool visible)
{
    if (m_checkboxColumnVisible == visible)
        return;

    if (visible)
        beginInsertColumns(QModelIndex(), 0, 0);
    else
        beginRemoveColumns(QModelIndex(), 0, 0);

    m_checkboxColumnVisible = visible;

    if (visible)
        endInsertColumns();
    else
        endRemoveColumns();
}

void RowSelectionModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    m_checkedRows.clear();
    QAbstractProxyModel::setSourceModel(sourceModel);

    m_connections.reset();

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset, this,
            [this]
            {
                beginResetModel();
                m_checkedRows.clear();
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::modelReset,
            this, &RowSelectionModel::endResetModel);

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this,
            [this](const QModelIndex& parent, int first, int last)
            {
                beginInsertRows(mapFromSource(parent), first, last);
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsInserted,
            this, &RowSelectionModel::endInsertRows);

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex& parent, int first, int last)
            {
                beginRemoveRows(mapFromSource(parent), first, last);
            });

    m_connections <<
        connect(sourceModel, &QAbstractItemModel::rowsRemoved,
            this, &RowSelectionModel::endRemoveRows);

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
            this, &RowSelectionModel::endMoveRows);

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
                    if (proxyPersistentIndex.column() == 0)
                        continue; //< Don't need to save this model native indexes.

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
                    changePersistentIndex(
                        m_proxyIndexes.at(i),
                        mapFromSource(m_layoutChangePersistentIndexes.at(i)));
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
                emit headerDataChanged(
                    orientation,
                    m_checkboxColumnVisible ? first + 1 : first,
                    m_checkboxColumnVisible ? last + 1 : last);
            });
}

QVariant RowSelectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    NX_ASSERT(section < columnCount());

    if (orientation != Qt::Orientation::Horizontal)
        return QVariant();

    if (section == 0 && m_checkboxColumnVisible)
    {
        if (role == Qt::CheckStateRole)
            return Qt::Unchecked;
    }

    return sourceModel()->headerData(
        section - (m_checkboxColumnVisible ? 1 : 0),
        orientation,
        role);
}

QVariant RowSelectionModel::data(const QModelIndex& index, int role) const
{
    NX_ASSERT(index.isValid());

    if (m_checkboxColumnVisible && index.column() == 0 && role == Qt::DisplayRole)
    {
        const QModelIndex checkboxIndex = sourceModel()->index(index.row(), 0);
        if (m_checkedRows.count(checkboxIndex) > 0)
            return Qt::Checked;
        return Qt::Unchecked;
    }

    return sourceModel()->data(mapToSource(index), role);
}

bool RowSelectionModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    NX_ASSERT(index.isValid());

    if (m_checkboxColumnVisible && index.column() == 0 && role == Qt::EditRole)
    {
        const QModelIndex checkboxIndex = sourceModel()->index(index.row(), 0);
        if (m_checkedRows.count(checkboxIndex) > 0)
            m_checkedRows.erase(QPersistentModelIndex(checkboxIndex));
        else
            m_checkedRows.insert(QPersistentModelIndex(checkboxIndex));

        emit dataChanged(index, index, {Qt::DisplayRole});
        return true;
    }

    return sourceModel()->setData(mapToSource(index), value, role);
}

QModelIndex RowSelectionModel::index(int row, int column, const QModelIndex& parent) const
{
    return createIndex(row, column);
}

QModelIndex RowSelectionModel::parent(const QModelIndex& index) const
{
    return {};
}

QModelIndex RowSelectionModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid() || !sourceModel())
        return {};

    NX_ASSERT(sourceIndex.row() < sourceModel()->rowCount());

    return index(
        sourceIndex.row(),
        sourceIndex.column() + (m_checkboxColumnVisible ? 1 : 0));
}

QModelIndex RowSelectionModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid() || !sourceModel())
        return {};

    NX_ASSERT(proxyIndex.row() < rowCount());

    return sourceModel()->index(
        proxyIndex.row(),
        proxyIndex.column() - (m_checkboxColumnVisible ? 1 : 0));
}

int RowSelectionModel::rowCount(const QModelIndex& parent) const
{
    if (!sourceModel() || parent.isValid())
        return 0;

    return sourceModel()->rowCount();
}

int RowSelectionModel::columnCount(const QModelIndex& parent) const
{
    if (!sourceModel() || parent.isValid())
        return 0;

    return sourceModel()->columnCount() + (m_checkboxColumnVisible ? 1 : 0);
}

void RowSelectionModel::sort(int column, Qt::SortOrder order)
{
    base_type::sort(m_checkboxColumnVisible ? column - 1 : column, order);
}

} // namespace nx::vms::client::desktop
