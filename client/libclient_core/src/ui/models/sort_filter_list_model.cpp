#include "sort_filter_list_model.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <utils/common/connective.h>

class QnSortFilterListModelPrivate: public Connective<QObject>
{
    using base_type = Connective<QObject>;

    Q_DECLARE_PUBLIC(QnSortFilterListModel)
    QnSortFilterListModel* q_ptr;

public:
    QnSortFilterListModelPrivate(QnSortFilterListModel* parent);

    void setTriggeringRoles(const QnSortFilterListModel::RolesSet& roles);

    QModelIndex sourceIndexForTargetRow(int row) const;
    void refresh();

    int rowCount() const;

private:
    void resetTargetModel();

    void handleSourceRowsInserted(
        const QModelIndex& parent,
        int first,
        int last);

    void handleSourceRowsAboutToBeRemoved(
        const QModelIndex& parent,
        int first,
        int last);

    void handleSourceRowsRemoved(
        const QModelIndex& parent,
        int first,
        int last);

    void handleSourceRowsMoved(
        const QModelIndex& parent,
        int start,
        int end,
        const QModelIndex& destination,
        int row);

    void handleSourceDataChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const QVector<int>& roles);

    void handleResetSourceModel();

private:
    void insertSourceRow(int sourceRow);

    void removeSourceRow(int sourceRow);

    bool isFilteredOut(int sourceRow) const;

    int indexToInsert(int sourceRow);

    void shiftMappedRows(int fromSourceRow, int difference);

private:
    using RowsList = QList<int>;

    RowsList m_mapped;
    QnSortFilterListModel::RolesSet m_triggeringRoles;
};

QnSortFilterListModelPrivate::QnSortFilterListModelPrivate(QnSortFilterListModel* parent):
    base_type(),
    q_ptr(parent)
{
}

void QnSortFilterListModelPrivate::resetTargetModel()
{
    Q_Q(QnSortFilterListModel);
    q->beginResetModel();
    m_mapped.clear();
    q->endResetModel();
}

void QnSortFilterListModelPrivate::setTriggeringRoles(const QnSortFilterListModel::RolesSet& roles)
{
    m_triggeringRoles = roles;
}

void QnSortFilterListModelPrivate::refresh()
{
    Q_Q(QnSortFilterListModel);

    const auto sourceSize = q->sourceModel()->rowCount();
    if (!sourceSize)
        return;

    for (int sourceRow = 0; sourceRow != sourceSize; ++sourceRow)
    {
        const int currentIndex = m_mapped.indexOf(sourceRow);
        const bool shouldBeFilteredOut = isFilteredOut(sourceRow);
        const bool currentFilteredOut = (currentIndex == -1);

        if (shouldBeFilteredOut != currentFilteredOut)
        {
            if (shouldBeFilteredOut)
                removeSourceRow(sourceRow);
            else
                insertSourceRow(sourceRow); // Here row is placed in right position.

            continue;
        }
        else if (currentIndex == -1) //< It is new row.
        {
            insertSourceRow(sourceRow);
            continue;
        }

        const int newIndex = indexToInsert(sourceRow);
        if (newIndex == currentIndex)
            continue; //< Row is in right place.

        const int updatedNewIndex = newIndex + (currentIndex < newIndex ? -1 : 0);

        Q_Q(QnSortFilterListModel);
        q->beginMoveRows(QModelIndex(), currentIndex, currentIndex, QModelIndex(), newIndex);
        m_mapped.removeAt(currentIndex);
        m_mapped.insert(m_mapped.begin() + updatedNewIndex, sourceRow);
        q->endMoveRows();
    }
}

int QnSortFilterListModelPrivate::rowCount() const
{
    return m_mapped.size();
}

bool QnSortFilterListModelPrivate::isFilteredOut(int sourceRow) const
{
    Q_Q(const QnSortFilterListModel);
    return !q->filterAcceptsRow(sourceRow, QModelIndex());
}

int QnSortFilterListModelPrivate::indexToInsert(int sourceRow)
{
    const auto it = std::lower_bound(m_mapped.begin(), m_mapped.end(), sourceRow,
        [this](int left, int right)
        {
            Q_Q(QnSortFilterListModel);
            const auto model = q->sourceModel();
            return (model
                ? q->lessThan(model->index(left, 0), model->index(right, 0))
                : left < right);
        });
    return std::distance(m_mapped.begin(), it);
}

void QnSortFilterListModelPrivate::insertSourceRow(int sourceRow)
{
    if (isFilteredOut(sourceRow))
        return;

    const auto mappedIndex = m_mapped.indexOf(sourceRow);
    if (mappedIndex != -1)
    {
        NX_ASSERT(false, "Can't insert row second time");
        return;
    }

    Q_Q(QnSortFilterListModel);
    const auto index = indexToInsert(sourceRow);
    q->beginInsertRows(QModelIndex(), index, index);
    m_mapped.insert(index, sourceRow);
    q->endInsertRows();
}

void QnSortFilterListModelPrivate::removeSourceRow(int sourceRow)
{
    const auto mappedIndex = m_mapped.indexOf(sourceRow);
    if (mappedIndex == -1)
        return; //< Row is filtered out.

    Q_Q(QnSortFilterListModel);
    q->beginRemoveRows(QModelIndex(), mappedIndex, mappedIndex);
    m_mapped.removeAt(mappedIndex);
    q->endRemoveRows();
}

void QnSortFilterListModelPrivate::shiftMappedRows(int minSourceRow, int difference)
{
    for(auto& row: m_mapped)
    {
        if (row >= minSourceRow)
            row += difference;
    }
}

void QnSortFilterListModelPrivate::handleSourceRowsInserted(
    const QModelIndex& parent,
    int first,
    int last)
{
    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid())
        return;

    /**
     * Since we have new rows in the source model, we have to increase all indicies
     * after first by (last - first + 1) value.
     */
    const int difference = (last - first + 1);
    shiftMappedRows(first, difference);

    for (int row = first; row <= last; ++row)
        insertSourceRow(row);
}

void QnSortFilterListModelPrivate::handleSourceRowsAboutToBeRemoved(
    const QModelIndex& parent,
    int first,
    int last)
{
    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid())
        return;

    for (int row = first; row <= last; ++row)
        removeSourceRow(row);
}

void QnSortFilterListModelPrivate::handleSourceRowsRemoved(
    const QModelIndex& parent,
    int first,
    int last)
{
    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid())
        return;

    const auto kRemoveDifference = -(first - last + 1);
    shiftMappedRows(last + 1, kRemoveDifference);
}

void QnSortFilterListModelPrivate::handleSourceRowsMoved(
    const QModelIndex& parent,
    int /* start */,
    int /* end */,
    const QModelIndex& destination,
    int /* row */)
{
    NX_ASSERT(!parent.isValid() && !destination.isValid(),
        "QnSortFilterListModel works only with flat lists models");
    if (parent.isValid() || destination.isValid())
        return;

    // TODO: #ynikitenkov support handling of moving items
    NX_ASSERT(false, "QnSortFilterListModel does not handle moves yet");
}

void QnSortFilterListModelPrivate::handleSourceDataChanged(
    const QModelIndex& topLeft,
    const QModelIndex& bottomRight,
    const QVector<int>& roles)
{
    Q_Q(QnSortFilterListModel);
    const auto model = q->sourceModel();
    if (!model)
        return;

    if (!topLeft.isValid() || !bottomRight.isValid())
    {
        NX_ASSERT(false, "Invalid indicies");
        return;
    }

    if (topLeft.column() || bottomRight.column())
    {
        NX_ASSERT(false, "QnSortFilterListModel supports only flat lists models");
        return;
    }

    for(int sourceRow = topLeft.row(); sourceRow <= bottomRight.row(); ++sourceRow)
    {
        const auto sourceIndex = model->index(sourceRow, 0);
        if (!sourceIndex.isValid())
            continue;

        const auto targetIndex = q->mapFromSource(sourceIndex);
        if (targetIndex.isValid())
            emit q->dataChanged(targetIndex, targetIndex, roles);
    }

    if (m_triggeringRoles.isEmpty()
        || m_triggeringRoles.intersects(roles.toList().toSet()))
    {
        refresh();
    }
}

void QnSortFilterListModelPrivate::handleResetSourceModel()
{
    resetTargetModel();
    refresh();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

QnSortFilterListModel::QnSortFilterListModel(QObject* parent):
    base_type(parent),
    d_ptr(new QnSortFilterListModelPrivate(this))
{
}

QnSortFilterListModel::~QnSortFilterListModel()
{
}

void QnSortFilterListModel::setSourceModel(QAbstractItemModel* model)
{
    const auto currentModel = sourceModel();
    if (model == currentModel)
        return;

    Q_D(QnSortFilterListModel);
    if (currentModel)
    {
        currentModel->disconnect(d);
        d->resetTargetModel();
    }

    base_type::setSourceModel(model);

    if (!model)
        return;

    connect(model, &QAbstractListModel::rowsInserted,
        d, &QnSortFilterListModelPrivate::handleSourceRowsInserted);

    connect(model, &QAbstractListModel::rowsAboutToBeRemoved,
        d, &QnSortFilterListModelPrivate::handleSourceRowsAboutToBeRemoved);
    connect(model, &QAbstractListModel::rowsRemoved,
        d, &QnSortFilterListModelPrivate::handleSourceRowsRemoved);
    connect(model, &QAbstractListModel::rowsMoved,
        d, &QnSortFilterListModelPrivate::handleSourceRowsMoved);
    connect(model, &QAbstractListModel::dataChanged,
        d, &QnSortFilterListModelPrivate::handleSourceDataChanged);
    connect(model, &QAbstractListModel::modelReset,
        d, &QnSortFilterListModelPrivate::handleResetSourceModel);

    //TODO: #ynikitenkov Make filling of model with data in reset model mode.
    d->refresh();
}

void QnSortFilterListModel::setTriggeringRoles(const RolesSet& roles)
{
    Q_D(QnSortFilterListModel);
    d->setTriggeringRoles(roles);
}

void QnSortFilterListModel::forceUpdate()
{
    Q_D(QnSortFilterListModel);
    d->refresh();
}

bool QnSortFilterListModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    return (sourceLeft.row() < sourceRight.row());
}

bool QnSortFilterListModel::filterAcceptsRow(
    int /* sourceRow */,
    const QModelIndex& /* sourceParent */) const
{
    return true;
}

QModelIndex QnSortFilterListModel::mapToSource(const QModelIndex& proxyIndex) const
{
    Q_D(const QnSortFilterListModel);

    const int row = proxyIndex.row();
    const auto model = sourceModel();
    if (!model || (row < 0) || (row >= d->m_mapped.size()))
        return QModelIndex();

    const auto sourceRow = d->m_mapped[row];
    return model->index(sourceRow, 0);
}

QModelIndex QnSortFilterListModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    Q_D(const QnSortFilterListModel);

    const int sourceRow = sourceIndex.row();
    const auto model = sourceModel();
    if (!model || (sourceRow < 0) || (sourceRow >= model->rowCount()))
        return QModelIndex();

    const int targetRow = d->m_mapped.indexOf(sourceRow);
    return (targetRow == -1 ? QModelIndex() : index(targetRow, 0));
}

QModelIndex QnSortFilterListModel::index(
    int row,
    int column,
    const QModelIndex& parent) const
{
    if (parent.isValid() || (column > 1))
    {
        NX_ASSERT(false, "QnSortFilterListModel supports is a flat list model");
        return QModelIndex();
    }

    return createIndex(row, column, nullptr);
}

QModelIndex QnSortFilterListModel::parent(const QModelIndex& /* child */) const
{
    return QModelIndex(); //< For list models parent is always empty.
}

int QnSortFilterListModel::rowCount(const QModelIndex& /* parent */) const
{
    Q_D(const QnSortFilterListModel);
    return d->rowCount();
}

int QnSortFilterListModel::columnCount(const QModelIndex& /*parent */) const
{
    return 1;
}

QHash<int, QByteArray> QnSortFilterListModel::roleNames() const
{
    const auto model = sourceModel();
    return (model ? model->roleNames() : QHash<int, QByteArray>());
}
