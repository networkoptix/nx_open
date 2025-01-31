// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "concatenation_list_model.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

ConcatenationListModel::ConcatenationListModel(QObject* parent):
    base_type(parent)
{
}

ConcatenationListModel::ConcatenationListModel(
    const std::initializer_list<QAbstractListModel*>& models,
    QObject* parent)
    :
    ConcatenationListModel(parent)
{
    setModels(models);
}

ConcatenationListModel::~ConcatenationListModel()
{
}

QList<QAbstractListModel*> ConcatenationListModel::models() const
{
    return m_models;
}

void ConcatenationListModel::setModels(const QList<QAbstractListModel*>& models)
{
    m_modelConnections = {};
    m_models = models;

    NX_VERBOSE(this, "Models set to: %1", m_models);

    for (const auto model: m_models)
        connectToModel(model);
}

QModelIndex ConcatenationListModel::mapToSource(const QModelIndex& index) const
{
    if (index.parent().isValid() || index.model() != this)
        return QModelIndex();

    int row = index.row();
    for (const auto model: m_models)
    {
        const int count = sourceRowCount(model);
        if (row < count)
            return model->index(row);

        row -= count;
    }

    return QModelIndex();
}

QModelIndex ConcatenationListModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    return sourceIndex.isValid()
        ? index(mapFromSourceRow(sourceIndex.model(), sourceIndex.row()))
        : QModelIndex();
}

int ConcatenationListModel::sourceModelPosition(const QAbstractItemModel* sourceModel) const
{
    int position = 0;
    for (const auto model: m_models)
    {
        if (model == sourceModel)
            return position;

        position += sourceRowCount(model);
    }

    NX_ASSERT(false, "Source model is not in the list");
    return -1;
}

int ConcatenationListModel::mapFromSourceRow(const QAbstractItemModel* sourceModel, int row) const
{
    const auto base = sourceModelPosition(sourceModel);
    return base >= 0 ? (base + row) : -1;
}

int ConcatenationListModel::rowCount(const QModelIndex& parent) const
{
    return std::accumulate(m_models.cbegin(), m_models.cend(), 0,
        [this](int count, const QAbstractListModel* model)
        {
            return count + sourceRowCount(model);
        });
}

int ConcatenationListModel::sourceRowCount(const QAbstractItemModel* sourceModel) const
{
    return sourceModel != m_ignoredModel ? sourceModel->rowCount() : 0;
}

bool ConcatenationListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid())
        return false;

    const auto sourceCounts =
        [this]() -> QString
        {
            QStringList result;
            for (auto model: m_models)
                result.push_back(QString::number(sourceRowCount(model)));
            return result.join(", ");
        };

    NX_VERBOSE(this, "Remove %1 row(s) at index %2, total count is %3 (%4), ignoredModel=%5",
        count, row, rowCount(), sourceCounts(), m_ignoredModel);

    if (row < 0 || count < 0 || row + count > rowCount())
    {
        NX_ASSERT(false, "Rows are out of range");
        return false;
    }

    if (count == 0)
        return true;

    bool result = true;

    int startRow = 0;
    for (auto model: m_models)
    {
        const auto sourceRows = sourceRowCount(model);
        if (row >= startRow + sourceRows)
        {
            startRow += sourceRows;
            continue;
        }

        NX_ASSERT(row >= startRow);

        const auto localFirst = row - startRow;
        const auto localCount = qMin(localFirst + count, sourceRows) - localFirst;

        NX_VERBOSE(this, "Removing %1 source row(s) at index %2 from %3",
            localCount, localFirst, model);

        result = model->removeRows(localFirst, localCount) && result;

        count -= localCount;
        if (count == 0)
            break;

        startRow += sourceRowCount(model); //< Changed after removal.
        row = startRow;
    }

    // Returns true only if all requested rows were removed.
    // This case isn't documented, but seems consistent with QSortFilterProxyModel implementation.
    return result;
}

void ConcatenationListModel::connectToModel(QAbstractListModel* model)
{
    NX_ASSERT(model);

    m_modelConnections << connect(model, &QObject::destroyed, this,
        [this, model]() { m_models.removeAll(model); }); //< Should happen only during destruction.

    m_modelConnections << connect(model, &QAbstractItemModel::modelAboutToBeReset,
        this, &ConcatenationListModel::sourceModelAboutToBeReset);

    m_modelConnections << connect(model, &QAbstractItemModel::modelReset,
        this, &ConcatenationListModel::sourceModelReset);

    m_modelConnections << connect(model, &QAbstractItemModel::rowsAboutToBeInserted,
        this, &ConcatenationListModel::sourceRowsAboutToBeInserted);

    m_modelConnections << connect(model, &QAbstractItemModel::rowsInserted,
        this, &ConcatenationListModel::sourceRowsInserted);

    m_modelConnections << connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &ConcatenationListModel::sourceRowsAboutToBeRemoved);

    m_modelConnections << connect(model, &QAbstractItemModel::rowsRemoved,
        this, &ConcatenationListModel::sourceRowsRemoved);

    m_modelConnections << connect(model, &QAbstractItemModel::rowsAboutToBeMoved,
        this, &ConcatenationListModel::sourceRowsAboutToBeMoved);

    m_modelConnections << connect(model, &QAbstractItemModel::rowsMoved,
        this, &ConcatenationListModel::sourceRowsMoved);

    m_modelConnections << connect(model, &QAbstractItemModel::layoutAboutToBeChanged,
        this, &ConcatenationListModel::sourceLayoutAboutToBeChanged);

    m_modelConnections << connect(model, &QAbstractItemModel::layoutChanged,
        this, &ConcatenationListModel::sourceLayoutChanged);

    m_modelConnections << connect(model, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
        {
            emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
        });
}

void ConcatenationListModel::sourceModelAboutToBeReset()
{
    const auto model = qobject_cast<QAbstractListModel*>(sender());
    NX_VERBOSE(this, "Source model about to be reset: %1", model);

    NX_ASSERT(model && !m_ignoredModel);
    const auto count = model->rowCount();
    if (!count)
    {
        m_ignoredModel = model;
        return; //< No rows removed.
    }

    const auto index = m_models.indexOf(model);
    const auto first = sourceModelPosition(model);
    NX_ASSERT(index >= 0 && first >= 0);

    beginRemoveRows(QModelIndex(), first, first + count - 1);
    m_ignoredModel = model;
    endRemoveRows();
}

void ConcatenationListModel::sourceModelReset()
{
    const auto model = qobject_cast<QAbstractListModel*>(sender());
    NX_VERBOSE(this, "Source model reset: %1", model);

    NX_ASSERT(model && m_ignoredModel);
    const auto count = model->rowCount();
    if (!count)
    {
        m_ignoredModel = nullptr;
        return; //< No rows inserted.
    }

    const auto index = m_models.indexOf(model);
    const auto first = sourceModelPosition(model);
    NX_ASSERT(index >= 0 && first >= 0);

    beginInsertRows(QModelIndex(), first, first + count - 1);
    m_ignoredModel = nullptr;
    endInsertRows();
}

void ConcatenationListModel::sourceRowsAboutToBeInserted(const QModelIndex& /*parent*/,
    int first, int last)
{
    const auto model = qobject_cast<const QAbstractListModel*>(sender());
    NX_VERBOSE(this, "Source rows (%1-%2) about to be inserted: %3", first, last, model);
    NX_ASSERT(model);
    beginInsertRows(QModelIndex(), mapFromSourceRow(model, first), mapFromSourceRow(model, last));
}

void ConcatenationListModel::sourceRowsInserted()
{
    NX_VERBOSE(this, "Source rows inserted: %1", sender());
    endInsertRows();
}

void ConcatenationListModel::sourceRowsAboutToBeRemoved(const QModelIndex& /*parent*/,
    int first, int last)
{
    const auto model = qobject_cast<const QAbstractListModel*>(sender());
    NX_VERBOSE(this, "Source rows (%1-%2) about to be removed: %3", first, last, model);
    NX_ASSERT(model);
    beginRemoveRows(QModelIndex(), mapFromSourceRow(model, first), mapFromSourceRow(model, last));
}

void ConcatenationListModel::sourceRowsRemoved()
{
    NX_VERBOSE(this, "Source rows removed: %1", sender());
    endRemoveRows();
}

void ConcatenationListModel::sourceRowsAboutToBeMoved(const QModelIndex& /*sourceParent*/,
    int sourceFirst, int sourceLast, const QModelIndex& /*destinationParent*/, int destinationRow)
{
    const auto model = qobject_cast<const QAbstractListModel*>(sender());
    NX_VERBOSE(this, "Source rows (%1-%2) about to be moved to %3: %4", sourceFirst, sourceLast,
        destinationRow, model);
    NX_ASSERT(model);
    beginMoveRows(
        QModelIndex(), mapFromSourceRow(model, sourceFirst), mapFromSourceRow(model, sourceLast),
        QModelIndex(), mapFromSourceRow(model, destinationRow));
}

void ConcatenationListModel::sourceRowsMoved()
{
    NX_VERBOSE(this, "Source rows moved: %1", sender());
    endMoveRows();
}

void ConcatenationListModel::sourceLayoutAboutToBeChanged(
    const QList<QPersistentModelIndex>& /*parents*/,
    QAbstractItemModel::LayoutChangeHint hint)
{
    NX_VERBOSE(this, "Source layout about to be changed (%1): %2", hint, sender());
    emit layoutAboutToBeChanged({}, hint);

    NX_ASSERT(m_changingIndices.empty());
    m_changingIndices.clear(); //< Just in case.

    const auto persistent = persistentIndexList();
    m_changingIndices.reserve(persistent.size());

    for (const QModelIndex& index: persistent)
    {
        const auto sourceIndex = mapToSource(index);
        if (sourceIndex.model() == sender()) //< Don't need to process them all.
            m_changingIndices << PersistentIndexPair(index, sourceIndex);
    }
}

void ConcatenationListModel::sourceLayoutChanged(
    const QList<QPersistentModelIndex>& /*parents*/,
    QAbstractItemModel::LayoutChangeHint hint)
{
    NX_VERBOSE(this, "Source layout changed (%1): %2", hint, sender());

    for (const auto& changed: m_changingIndices)
        changePersistentIndex(changed.first, mapFromSource(changed.second));

    m_changingIndices.clear();
    emit layoutChanged({}, hint);
}

} // namespace nx::vms::client::core
