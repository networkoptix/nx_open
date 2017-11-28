#include "subset_list_model.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/raii_guard.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static bool equals(const QModelIndex& left, const QModelIndex& right)
{
    return left.isValid() ? left == right : !right.isValid();
}

} // namespace

SubsetListModel::SubsetListModel(QObject* parent): base_type(parent)
{
}

SubsetListModel::SubsetListModel(
    QAbstractItemModel* sourceModel,
    int sourceColumn,
    const QModelIndex& sourceRoot,
    QObject* parent)
    :
    SubsetListModel(parent)
{
    setSource(sourceModel, sourceColumn, sourceRoot);
}

SubsetListModel::~SubsetListModel()
{
    m_modelConnections.reset();
}

QAbstractItemModel* SubsetListModel::sourceModel() const
{
    return m_sourceModel;
}

QModelIndex SubsetListModel::sourceRoot() const
{
    return m_sourceRoot;
}

int SubsetListModel::sourceColumn() const
{
    return m_sourceColumn;
}

void SubsetListModel::setSource(
    QAbstractItemModel* sourceModel,
    int sourceColumn,
    const QModelIndex& sourceRoot)
{
    const bool sameParams = m_sourceColumn != sourceColumn || m_sourceRoot != sourceRoot;
    if (sameParams && m_sourceModel == sourceModel)
        return;

    ScopedReset reset(this);

    m_sourceColumn = sourceColumn;
    m_sourceRoot = sourceRoot;

    if (m_sourceModel == sourceModel)
        return;

    m_modelConnections.reset();
    m_sourceModel = sourceModel;

    if (m_sourceModel)
        connectToModel(m_sourceModel);
}

QModelIndex SubsetListModel::mapToSource(const QModelIndex& index) const
{
    if (!m_sourceModel || !index.isValid() || index.model() != this)
        return QModelIndex();

    return m_sourceModel->index(index.row(), m_sourceColumn, m_sourceRoot);
}

QModelIndex SubsetListModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid() || sourceIndex.model() != m_sourceModel
        || sourceIndex.column() != m_sourceColumn || sourceIndex.parent() != m_sourceRoot)
    {
        return QModelIndex();
    }

    return index(sourceIndex.row());
}

void SubsetListModel::connectToModel(QAbstractItemModel* model)
{
    NX_EXPECT(model);

    m_modelConnections.reset(new QnDisconnectHelper());

    *m_modelConnections << connect(model, &QObject::destroyed, this,
        [this]()
        {
            m_sourceModel = nullptr;
            m_sourceColumn = 0;
            m_sourceRoot = QPersistentModelIndex();
            ScopedReset reset(this);
        });

    *m_modelConnections << connect(model, &QAbstractItemModel::modelAboutToBeReset,
        this, &SubsetListModel::beginResetModel);
    *m_modelConnections << connect(model, &QAbstractItemModel::modelReset,
        this, &SubsetListModel::endResetModel);

    *m_modelConnections << connect(model, &QAbstractItemModel::headerDataChanged,
        this, &SubsetListModel::headerDataChanged);

    *m_modelConnections << connect(model, &QAbstractItemModel::rowsAboutToBeInserted,
        this, &SubsetListModel::sourceRowsAboutToBeInserted);
    *m_modelConnections << connect(model, &QAbstractItemModel::rowsInserted,
        this, &SubsetListModel::sourceRowsInserted);
    *m_modelConnections << connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &SubsetListModel::sourceRowsAboutToBeRemoved);
    *m_modelConnections << connect(model, &QAbstractItemModel::rowsRemoved,
        this, &SubsetListModel::sourceRowsRemoved);
    *m_modelConnections << connect(model, &QAbstractItemModel::rowsAboutToBeMoved,
        this, &SubsetListModel::sourceRowsAboutToBeMoved);
    *m_modelConnections << connect(model, &QAbstractItemModel::rowsMoved,
        this, &SubsetListModel::sourceRowsMoved);

    *m_modelConnections << connect(model, &QAbstractItemModel::columnsInserted,
        this, &SubsetListModel::sourceColumnsInserted);
    *m_modelConnections << connect(model, &QAbstractItemModel::columnsRemoved,
        this, &SubsetListModel::sourceColumnsRemoved);
    *m_modelConnections << connect(model, &QAbstractItemModel::columnsMoved,
        this, &SubsetListModel::sourceColumnsMoved);

    *m_modelConnections << connect(model, &QAbstractItemModel::layoutAboutToBeChanged,
        this, &SubsetListModel::sourceLayoutAboutToBeChanged);
    *m_modelConnections << connect(model, &QAbstractItemModel::layoutChanged,
        this, &SubsetListModel::sourceLayoutChanged);

    *m_modelConnections << connect(model, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
        {
            if (topLeft.column() <= m_sourceColumn && bottomRight.column() >= m_sourceColumn
                && equals(topLeft.parent(), m_sourceRoot))
            {
                emit dataChanged(index(topLeft.row()), index(bottomRight.row()), roles);
            }
        });

    *m_modelConnections << connect(model, &QAbstractItemModel::headerDataChanged, this,
        [this](Qt::Orientation orientation, int first, int last)
        {
            if (orientation == Qt::Vertical || (m_sourceColumn >= first && m_sourceColumn <= last))
                emit headerDataChanged(orientation, first, last);
        });
}

void SubsetListModel::sourceRowsAboutToBeInserted(const QModelIndex& parent, int first, int last)
{
    if (equals(parent, m_sourceRoot))
        beginInsertRows(QModelIndex(), first, last);
}

void SubsetListModel::sourceRowsInserted(const QModelIndex& parent)
{
    if (equals(parent, m_sourceRoot))
        endInsertRows();
}

void SubsetListModel::sourceRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    if (equals(parent, m_sourceRoot))
        beginRemoveRows(QModelIndex(), first, last);
}

void SubsetListModel::sourceRowsRemoved(const QModelIndex& parent)
{
    if (equals(parent, m_sourceRoot))
        endRemoveRows();
}

void SubsetListModel::sourceRowsAboutToBeMoved(const QModelIndex& sourceParent, int sourceFirst,
    int sourceLast, const QModelIndex& destinationParent, int destinationRow)
{
    if (equals(sourceParent, m_sourceRoot))
    {
        if (equals(destinationParent, m_sourceRoot))
            beginMoveRows(QModelIndex(), sourceFirst, sourceLast, QModelIndex(), destinationRow);
        else
            beginRemoveRows(QModelIndex(), sourceFirst, sourceLast);
    }
    else if (equals(destinationParent, m_sourceRoot))
    {
        const auto destinationEnd = destinationRow + sourceLast - sourceFirst;
        beginInsertRows(QModelIndex(), destinationRow, destinationEnd);
    }
}

void SubsetListModel::sourceRowsMoved(const QModelIndex& sourceParent, int /*sourceFirst*/,
    int /*sourceLast*/, const QModelIndex& destinationParent, int /*destinationRow*/)
{
    if (equals(sourceParent, m_sourceRoot))
    {
        if (equals(destinationParent, m_sourceRoot))
            endMoveRows();
        else
            endRemoveRows();
    }
    else if (equals(destinationParent, m_sourceRoot))
    {
        endInsertRows();
    }
}

void SubsetListModel::sourceColumnsInserted(const QModelIndex& parent, int first, int last)
{
    if (equals(parent, m_sourceRoot) && first <= m_sourceColumn)
        entireColumnChanged();
}

void SubsetListModel::sourceColumnsRemoved(const QModelIndex& parent, int first, int last)
{
    if (equals(parent, m_sourceRoot) && first <= m_sourceColumn)
        entireColumnChanged();
}

void SubsetListModel::sourceColumnsMoved(const QModelIndex& sourceParent, int sourceFirst,
    int sourceLast, const QModelIndex& destinationParent, int destinationRow)
{
    if ((equals(sourceParent, m_sourceRoot) && sourceFirst <= m_sourceColumn)
        || (equals(destinationParent, m_sourceRoot) && destinationRow <= m_sourceColumn))
    {
        entireColumnChanged();
    }
}

void SubsetListModel::sourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex>& parents,
    QAbstractItemModel::LayoutChangeHint hint)
{
    if (!parents.isEmpty() && !parents.contains(m_sourceRoot))
        return;

    if (hint == QAbstractItemModel::HorizontalSortHint)
        return;

    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);

    NX_ASSERT(m_changingIndices.empty());
    m_changingIndices.clear(); //< Just in case.

    const auto persistent = persistentIndexList();
    m_changingIndices.reserve(persistent.size());

    for (const auto& index: persistent)
        m_changingIndices << PersistentIndexPair(index, mapToSource(index));
}

void SubsetListModel::sourceLayoutChanged(const QList<QPersistentModelIndex>& parents,
    QAbstractItemModel::LayoutChangeHint hint)
{
    if (!parents.isEmpty() && !parents.contains(m_sourceRoot))
        return;

    if (hint != QAbstractItemModel::HorizontalSortHint)
    {
        for (const auto& changed: m_changingIndices)
            changePersistentIndex(changed.first, mapFromSource(changed.second));

        m_changingIndices.clear();
        emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
    }

    if (hint != QAbstractItemModel::VerticalSortHint)
        entireColumnChanged(); //< Emit dataChanged in case columns were rearranged.
}

void SubsetListModel::entireColumnChanged()
{
    const auto count = rowCount();
    if (count > 0)
        emit dataChanged(index(0), index(count - 1));
}

int SubsetListModel::rowCount(const QModelIndex& parent) const
{
    return m_sourceModel && !parent.isValid() ? m_sourceModel->rowCount(m_sourceRoot) : 0;
}

bool SubsetListModel::insertRows(int row, int count, const QModelIndex& parent)
{
    return m_sourceModel && !parent.isValid()
        ? m_sourceModel->insertRows(row, count, m_sourceRoot)
        : false;
}

bool SubsetListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    return m_sourceModel && !parent.isValid()
        ? m_sourceModel->removeRows(row, count, m_sourceRoot)
        : false;
}

bool SubsetListModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
    const QModelIndex& destinationParent, int destinationRow)
{
    return m_sourceModel && !sourceParent.isValid() && !destinationParent.isValid()
        ? m_sourceModel->moveRows(m_sourceRoot, sourceRow, count, m_sourceRoot, destinationRow)
        : false;
}

} // namespace desktop
} // namespace client
} // namespace nx
