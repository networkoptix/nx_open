#include "concatenation_list_model.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

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
    m_modelConnections.reset();
}

QList<QAbstractListModel*> ConcatenationListModel::models() const
{
    return m_models;
}

void ConcatenationListModel::setModels(const QList<QAbstractListModel*>& models)
{
    m_modelConnections.reset(new QnDisconnectHelper());
    m_models = models;

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

    NX_ASSERT(false, Q_FUNC_INFO, "Source model is not in the list");
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

    if (row < 0 || count < 0 || row + count > rowCount())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Rows are out of range");
        return false;
    }

    if (count == 0)
        return true;

    int startRow = 0;
    for (auto model: m_models)
    {
        const auto sourceRows = sourceRowCount(model);
        if (row >= startRow + sourceRows)
        {
            startRow += sourceRows;
            continue;
        }

        NX_EXPECT(row >= startRow);

        const auto localFirst = row - startRow;
        const auto localCount = qMin(localFirst + count, sourceRows) - localFirst;

        if (!model->removeRows(localFirst, localCount))
            return false;

        count -= localCount;
        if (count == 0)
            break;

        startRow += sourceRowCount(model); //< Changed after removal.
        row = startRow;
    }

    return true;
}

void ConcatenationListModel::connectToModel(QAbstractListModel* model)
{
    NX_EXPECT(model);

    *m_modelConnections << connect(model, &QObject::destroyed, this,
        [this, model]() { m_models.removeAll(model); }); //< Should happen only during destruction.

    *m_modelConnections << connect(model, &QAbstractItemModel::modelAboutToBeReset,
        this, &ConcatenationListModel::sourceModelAboutToBeReset);

    *m_modelConnections << connect(model, &QAbstractItemModel::modelReset,
        this, &ConcatenationListModel::sourceModelReset);

    *m_modelConnections << connect(model, &QAbstractItemModel::rowsAboutToBeInserted,
        this, &ConcatenationListModel::sourceRowsAboutToBeInserted);

    *m_modelConnections << connect(model, &QAbstractItemModel::rowsInserted,
        this, &ConcatenationListModel::sourceRowsInserted);

    *m_modelConnections << connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &ConcatenationListModel::sourceRowsAboutToBeRemoved);

    *m_modelConnections << connect(model, &QAbstractItemModel::rowsRemoved,
        this, &ConcatenationListModel::sourceRowsRemoved);

    *m_modelConnections << connect(model, &QAbstractItemModel::rowsAboutToBeMoved,
        this, &ConcatenationListModel::sourceRowsAboutToBeMoved);

    *m_modelConnections << connect(model, &QAbstractItemModel::rowsMoved,
        this, &ConcatenationListModel::sourceRowsMoved);

    *m_modelConnections << connect(model, &QAbstractItemModel::layoutAboutToBeChanged,
        this, &ConcatenationListModel::sourceLayoutAboutToBeChanged);

    *m_modelConnections << connect(model, &QAbstractItemModel::layoutChanged,
        this, &ConcatenationListModel::sourceLayoutChanged);

    *m_modelConnections << connect(model, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
        {
            emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
        });
}

void ConcatenationListModel::sourceModelAboutToBeReset()
{
    const auto model = qobject_cast<QAbstractListModel*>(sender());
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
    NX_ASSERT(model);
    beginInsertRows(QModelIndex(), mapFromSourceRow(model, first), mapFromSourceRow(model, last));
}

void ConcatenationListModel::sourceRowsInserted()
{
    endInsertRows();
}

void ConcatenationListModel::sourceRowsAboutToBeRemoved(const QModelIndex& /*parent*/,
    int first, int last)
{
    const auto model = qobject_cast<const QAbstractListModel*>(sender());
    NX_ASSERT(model);
    beginRemoveRows(QModelIndex(), mapFromSourceRow(model, first), mapFromSourceRow(model, last));
}

void ConcatenationListModel::sourceRowsRemoved()
{
    endRemoveRows();
}

void ConcatenationListModel::sourceRowsAboutToBeMoved(const QModelIndex& /*sourceParent*/,
    int sourceFirst, int sourceLast, const QModelIndex& /*destinationParent*/, int destinationRow)
{
    const auto model = qobject_cast<const QAbstractListModel*>(sender());
    NX_ASSERT(model);
    beginMoveRows(
        QModelIndex(), mapFromSourceRow(model, sourceFirst), mapFromSourceRow(model, sourceLast),
        QModelIndex(), mapFromSourceRow(model, destinationRow));
}

void ConcatenationListModel::sourceRowsMoved()
{
    endMoveRows();
}

void ConcatenationListModel::sourceLayoutAboutToBeChanged()
{
    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);

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

void ConcatenationListModel::sourceLayoutChanged()
{
    for (const auto& changed: m_changingIndices)
        changePersistentIndex(changed.first, mapFromSource(changed.second));

    m_changingIndices.clear();
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

} // namespace desktop
} // namespace client
} // namespace nx
