#include "sorting_proxy_model.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

namespace {

const auto defaultFilteringPredicate =
    [](const QModelIndex& /* value */) -> bool
    {
        return true;
    };

const auto defaultSortingPredicate =
    [](int left, int right)
    {
        return left < right;
    };

} // unnamed namespace

//////////////////////////////////////////////////////////////////////////////////////////////////
class QnSortingProxyModelPrivate : public QObject
{
    using base_type = QObject;

    Q_DECLARE_PUBLIC(QnSortingProxyModel)
    QnSortingProxyModel *q_ptr;

public:
    QnSortingProxyModelPrivate(QnSortingProxyModel* parent);

    void setModel(QAbstractListModel* model);
    QAbstractListModel* model() const;

    void setSortingPred(const QnSortingProxyModel::SortingPredicate& pred);
    void setFilteringPred(const QnSortingProxyModel::FilteringPredicate& pred);
    void setTriggeringRoles(const QnSortingProxyModel::RolesList& roles);

    QModelIndex sourceIndexForTargetRow(int row) const;
    void invalidate();

    int rowCount() const;

private:
    void setCurrentModel(QAbstractListModel* model);
    void clearCurrentModel();

    void handleSourceRowsInserted(
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

private:
    void insertSourceRow(int sourceRow);

    void removeSourceRow(int sourceRow);

    bool isFilteredOut(int sourceRow) const;

    typedef QList<int> RowsList;
    RowsList::iterator positionToInsert(int sourceRow);

    void shiftMappedRows(int fromSourceRow, int difference);

private:
    RowsList m_mapped;
    QAbstractListModel* m_model = nullptr;
    QnSortingProxyModel::FilteringPredicate m_filterPred = defaultFilteringPredicate;
    std::function<bool (int, int)> m_sortingPred = defaultSortingPredicate;

    using RolesSet = QSet<int>;
    RolesSet m_triggeringRoles;
};

QnSortingProxyModelPrivate::QnSortingProxyModelPrivate(QnSortingProxyModel* parent):
    base_type(),
    q_ptr(parent)
{
}

void QnSortingProxyModelPrivate::setModel(QAbstractListModel* model)
{
    if (m_model == model)
        return;

    if (m_model)
        clearCurrentModel();

    setCurrentModel(model);
    invalidate();
}

QAbstractListModel* QnSortingProxyModelPrivate::model() const
{
    return m_model;
}

void QnSortingProxyModelPrivate::setSortingPred(
    const QnSortingProxyModel::SortingPredicate& pred)
{

    if (!pred)
    {
        m_sortingPred = defaultSortingPredicate;
        return;
    }

    m_sortingPred =
        [this, pred](int leftRow, int rightRow) -> bool
        {
            if (!m_model)
                return (leftRow < rightRow);

            return pred(m_model->index(leftRow), m_model->index(rightRow));
        };
    invalidate();
}

void QnSortingProxyModelPrivate::setFilteringPred(
    const QnSortingProxyModel::FilteringPredicate& pred)
{
    m_filterPred = pred;
    invalidate();
}

void QnSortingProxyModelPrivate::setTriggeringRoles(const QnSortingProxyModel::RolesList& roles)
{
    m_triggeringRoles = RolesSet::fromList(roles);
}

void QnSortingProxyModelPrivate::setCurrentModel(QAbstractListModel* model)
{
    m_model = model;
    if (!m_model)
        return;

    connect(m_model, &QAbstractListModel::rowsInserted,
        this, &QnSortingProxyModelPrivate::handleSourceRowsInserted);
    connect(m_model, &QAbstractListModel::rowsRemoved,
        this, &QnSortingProxyModelPrivate::handleSourceRowsRemoved);
    connect(m_model, &QAbstractListModel::rowsMoved,
        this, &QnSortingProxyModelPrivate::handleSourceRowsMoved);
    connect(m_model, &QAbstractListModel::dataChanged,
        this, &QnSortingProxyModelPrivate::handleSourceDataChanged);
}

void QnSortingProxyModelPrivate::clearCurrentModel()
{
    if (!m_model)
        return;

    disconnect(m_model);

    Q_Q(QnSortingProxyModel);
    q->beginResetModel();
    m_mapped.clear();
    q->endResetModel();

    m_model = nullptr;
}

QModelIndex QnSortingProxyModelPrivate::sourceIndexForTargetRow(int row) const
{
    if (!m_model || (row < 0) || (row >= m_mapped.size()))
        return QModelIndex();

    const auto sourceRow = m_mapped[row];
    return m_model->index(sourceRow);
}

void QnSortingProxyModelPrivate::invalidate()
{
    if (!m_model)
        return;

    const auto sourceSize = m_model->rowCount();
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
                insertSourceRow(sourceRow); // Here row is placed in right position

            continue;
        }
        else if (currentIndex == -1) //< It is new row
        {
            insertSourceRow(sourceRow);
            continue;
        }

        const auto newPosition = positionToInsert(sourceRow);
        const int newIndex = std::distance(m_mapped.begin(), newPosition);
        if (newIndex == currentIndex)
            continue; //< Row is in right place

        const int updatedNewIndex = newIndex + (currentIndex < newIndex ? -1 : 0);

        Q_Q(QnSortingProxyModel);
        q->beginMoveRows(QModelIndex(), currentIndex, currentIndex, QModelIndex(), newIndex);
        m_mapped.removeAt(currentIndex);
        m_mapped.insert(m_mapped.begin() + updatedNewIndex, sourceRow);
        q->endMoveRows();
    }
}

int QnSortingProxyModelPrivate::rowCount() const
{
    return m_mapped.size();
}

bool QnSortingProxyModelPrivate::isFilteredOut(int sourceRow) const
{
    return (m_filterPred ? !m_filterPred(m_model->index(sourceRow)) : false);
}

QnSortingProxyModelPrivate::RowsList::iterator QnSortingProxyModelPrivate::positionToInsert(
    int sourceRow)
{
    return std::lower_bound(m_mapped.begin(), m_mapped.end(), sourceRow, m_sortingPred);
}

void QnSortingProxyModelPrivate::insertSourceRow(int sourceRow)
{
    if (isFilteredOut(sourceRow))
        return;

    const auto mappedIndex = m_mapped.indexOf(sourceRow);
    if (mappedIndex != -1)
    {
        NX_ASSERT(false, "Can't insert row second time");
        return;
    }

    Q_Q(QnSortingProxyModel);
    const auto pos = positionToInsert(sourceRow);
    const auto index = std::distance(m_mapped.begin(), pos);
    q->beginInsertRows(QModelIndex(), index, index);
    m_mapped.insert(pos, sourceRow);
    q->endInsertRows();
}

void QnSortingProxyModelPrivate::removeSourceRow(int sourceRow)
{
    const int index = m_mapped.indexOf(sourceRow);
    if (index == -1)
        return; //< Row is filtered out

    Q_Q(QnSortingProxyModel);
    q->beginRemoveRows(QModelIndex(), index, index);
    m_mapped.removeAt(index);
    q->endRemoveRows();
}

void QnSortingProxyModelPrivate::shiftMappedRows(int minSourceRow, int difference)
{
    std::for_each(m_mapped.begin(), m_mapped.end(),
        [minSourceRow, difference](int& row)
        {
            if (row >= minSourceRow)
                row += difference;
        });
}

void QnSortingProxyModelPrivate::handleSourceRowsInserted(
    const QModelIndex& parent,
    int first,
    int last)
{
    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid())
        return;

    // We have to increase all indicies after first by (last - first + 1) value.
    const int difference = (last - first + 1);
    shiftMappedRows(first, difference);

    for (int row = first; row <= last; ++row)
        insertSourceRow(row);
}

void QnSortingProxyModelPrivate::handleSourceRowsRemoved(
    const QModelIndex& parent,
    int first,
    int last)
{
    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid())
        return;

    for (int row = first; row <= last; ++row)
        removeSourceRow(row);

    // We have to decrease all indicies after first by (last - first + 1) value.
    const int difference = -(last - first + 1);
    shiftMappedRows(last, difference);
}

void QnSortingProxyModelPrivate::handleSourceRowsMoved(
    const QModelIndex& parent,
    int /* start */,
    int /* end */,
    const QModelIndex& destination,
    int /* row */)
{
    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    NX_ASSERT(!destination.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid() || destination.isValid())
        return;

    // TODO: #ynikitenkov support handling of moving items
    NX_ASSERT(false, "QnSortingProxyModel does not handle moves yet");
}

void QnSortingProxyModelPrivate::handleSourceDataChanged(
    const QModelIndex& topLeft,
    const QModelIndex& bottomRight,
    const QVector<int>& roles)
{
    if (!topLeft.isValid() || !bottomRight.isValid())
    {
        NX_ASSERT(false, "Invalid indicies");
        return;
    }

    if (topLeft.column() || bottomRight.column())
    {
        NX_ASSERT(false, "QnSortingProxyModel supports only flat lists models");
        return;
    }

    Q_Q(QnSortingProxyModel);
    for(int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {
        const int targetRow = m_mapped.indexOf(row);
        if (targetRow == -1)
            continue;

        const auto targetIndex = q->index(targetRow);
        emit q->dataChanged(targetIndex, targetIndex, roles);
    }

    if (m_triggeringRoles.isEmpty() ||
        !m_triggeringRoles.intersect(RolesSet::fromList(roles.toList())).isEmpty())
    {
        invalidate();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

QnSortingProxyModel::QnSortingProxyModel(QObject* parent):
    base_type(parent),
    d_ptr(new QnSortingProxyModelPrivate(this))
{
}

QnSortingProxyModel::~QnSortingProxyModel()
{
}

void QnSortingProxyModel::setSourceModel(QAbstractListModel* model)
{
    Q_D(QnSortingProxyModel);
    d->setModel(model);
}

QAbstractListModel* QnSortingProxyModel::sourceModel() const
{
    Q_D(const QnSortingProxyModel);
    return d->model();
}

void QnSortingProxyModel::setSortingPredicate(const SortingPredicate& pred)
{
    Q_D(QnSortingProxyModel);
    d->setSortingPred(pred);
}

void QnSortingProxyModel::setFilteringPredicate(const FilteringPredicate& pred)
{
    Q_D(QnSortingProxyModel);
    d->setFilteringPred(pred);
}

void QnSortingProxyModel::setTriggeringRoles(const RolesList& roles)
{
    Q_D(QnSortingProxyModel);
    d->setTriggeringRoles(roles);
}

void QnSortingProxyModel::forceUpdate()
{
    Q_D(QnSortingProxyModel);
    d->invalidate();
}

int QnSortingProxyModel::rowCount(const QModelIndex& /* parent */) const
{
    Q_D(const QnSortingProxyModel);
    return d->rowCount();
}

QVariant QnSortingProxyModel::data(const QModelIndex& index, int role) const
{
    Q_D(const QnSortingProxyModel);
    const auto modelIndex = d->sourceIndexForTargetRow(index.row());
    return modelIndex.data(role);
}

QHash<int, QByteArray> QnSortingProxyModel::roleNames() const
{
    Q_D(const QnSortingProxyModel);
    const auto model = d->model();
    return (model ? model->roleNames() : QHash<int, QByteArray>());
}
