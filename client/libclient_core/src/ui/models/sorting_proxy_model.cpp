#include "sorting_proxy_model.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

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

    QModelIndex sourceIndexForTargetRow(int row) const;
    void invalidate();

    int rowCount() const;

private:
    void setCurrentModel(QAbstractListModel* model);
    void clearCurrentModel();

    void applySort();
    void applyFilter();

    void handleSourceRowsInserted(
        const QModelIndex &parent,
        int first,
        int last);

    void handleSourceRowsRemoved(
        const QModelIndex &parent,
        int first,
        int last);

    void handleSourceRowsMoved(
        const QModelIndex &parent,
        int start,
        int end,
        const QModelIndex &destination,
        int row);

    void handleSourceDataChanged(
        const QModelIndex &topLeft,
        const QModelIndex &bottomRight,
        const QVector<int> &roles);

private:
    void insertFilteredRow(int row);

    void removeFilteredRow(int index);

    bool isFilteredOut(int row) const;

    typedef QList<int> RowsList;
    void shiftFilteredRows(const RowsList::iterator it, int difference);

private:
    RowsList m_filteredRows;
    RowsList m_rowsMapping;
    QAbstractListModel* m_model;
    QnSortingProxyModel::FilteringPredicate m_filterPred;
    QnSortingProxyModel::SortingPredicate m_sortingPred;
};

QnSortingProxyModelPrivate::QnSortingProxyModelPrivate(QnSortingProxyModel* parent):
    base_type(),

    q_ptr(parent),

    m_filteredRows(),
    m_rowsMapping(),
    m_model(nullptr),
    m_filterPred(),
    m_sortingPred()
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
    m_sortingPred = pred;
    invalidate();
}

void QnSortingProxyModelPrivate::setFilteringPred(
    const QnSortingProxyModel::FilteringPredicate& pred)
{
    m_filterPred = pred;
    invalidate();
}

void QnSortingProxyModelPrivate::setCurrentModel(QAbstractListModel* model)
{
    m_model = model;
    if (!m_model)
        return;

    namespace p = std::placeholders;
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

    disconnect(m_model, nullptr, this, nullptr);

    Q_Q(QnSortingProxyModel);
    q->beginResetModel();
    m_rowsMapping.clear();
    q->endResetModel();

    m_model = nullptr;
}

QModelIndex QnSortingProxyModelPrivate::sourceIndexForTargetRow(int row) const
{
    if (!m_model || (row < 0) || (row >= m_rowsMapping.size()))
        return QModelIndex();

    const auto sourceRow = m_rowsMapping[row];
    return m_model->index(sourceRow);
}

void QnSortingProxyModelPrivate::invalidate()
{
    applyFilter();
    applySort();
}

int QnSortingProxyModelPrivate::rowCount() const
{
    return m_rowsMapping.size();
}

void QnSortingProxyModelPrivate::applySort()
{
    if (!m_model)
        return;

    static const auto defaultPred =
        [](const QModelIndex& left, const QModelIndex& right)
        {
            return left.row() < right.row();
        };

    const auto modelItemsPred = (m_sortingPred ? m_sortingPred : defaultPred);
    const auto pred =
        [this, modelItemsPred](int leftRow, int rightRow)
        {
            return modelItemsPred(m_model->index(leftRow), m_model->index(rightRow));
        };

    auto sorted = m_filteredRows;
    std::sort(sorted.begin(), sorted.end(), pred);

    int currentIndex = 0;
    for (auto sortedRow: sorted)
    {
        const auto index = currentIndex++;
        if ((index < m_rowsMapping.size()) && (m_rowsMapping.at(index) == sortedRow))
            continue;   //< We don't need to move row - it is on its place

        Q_Q(QnSortingProxyModel);

        const auto oldIndex = m_rowsMapping.indexOf(sortedRow, index + 1);
        if (oldIndex == -1)
        {
            // Here we have added row
            q->beginInsertRows(QModelIndex(), index, index);
            const auto position = (m_rowsMapping.begin() + index);
            m_rowsMapping.insert(position, sortedRow);
            q->endInsertRows();
            continue;
        }
        else
        {
            // Here we have to move existing row.
            q->beginMoveRows(QModelIndex(), oldIndex, oldIndex, QModelIndex(), index);
            m_rowsMapping.removeAt(oldIndex);

            const auto position = (m_rowsMapping.begin() + index);
            m_rowsMapping.insert(position, sortedRow);
            q->endMoveRows();
        }
    }
}

void  QnSortingProxyModelPrivate::applyFilter()
{
    if (!m_model)
        return;

    const auto sourceSize = m_model->rowCount();
    if (!sourceSize)
        return;

    if (!m_filterPred && (m_filteredRows.size() == sourceSize))
        return; // All items are presented

    bool changed = false;
    for (int row = 0; row != sourceSize; ++row)
    {
        const int filteredIndex = m_filteredRows.indexOf(row);
        const bool filteredOutAlready = (filteredIndex == -1);
        const bool filteredOut = isFilteredOut(row);
        if (filteredOutAlready == filteredOut)
            continue;

        // Here row is restored or removed.
        changed = true;

        if (filteredOut)
            removeFilteredRow(filteredIndex);
        else
            insertFilteredRow(row);
    }

    if (changed)
        applySort();
}

bool QnSortingProxyModelPrivate::isFilteredOut(int row) const
{
    // Fake predicate restores all filtered out items
    static auto const defaultPredicate = [](const QModelIndex& /*index*/) { return true; };
    const auto pred = (m_filterPred ? m_filterPred : defaultPredicate);
    return !pred(m_model->index(row));
}

void QnSortingProxyModelPrivate::insertFilteredRow(int row)
{
    /**
     * We don't emit insert signal because it will be emitted
     * in sorting function with correct position.
     */
    if (isFilteredOut(row))
        return;

    const auto pos = std::lower_bound(m_filteredRows.begin(), m_filteredRows.end(), row);
    m_filteredRows.insert(pos, row);
}

void QnSortingProxyModelPrivate::removeFilteredRow(int index)
{
    const int row = m_filteredRows.at(index);
    m_filteredRows.removeAt(index);
    const auto mappedIndex = m_rowsMapping.indexOf(row);

    if (mappedIndex == -1)
    {
        NX_ASSERT(false, "Mapped index should exist according to filtered rows");
        return;
    }

    Q_Q(QnSortingProxyModel);
    q->beginRemoveRows(QModelIndex(), mappedIndex, mappedIndex);
    m_rowsMapping.removeAt(mappedIndex);
    q->endRemoveRows();
}

void QnSortingProxyModelPrivate::shiftFilteredRows(
    const RowsList::iterator posFrom,
    int difference)
{
    std::for_each(posFrom, m_filteredRows.end(),
        [difference](RowsList::value_type& row) { row += difference; });
}

void QnSortingProxyModelPrivate::handleSourceRowsInserted(
    const QModelIndex &parent,
    int first,
    int last)
{
    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid())
        return;

    /**
     * We have to just increase all indicies after first by (last - first + 1) value.
     * All other work will be done in applyFilter func
     */
    const int difference = (last - first + 1);
    const auto pos = std::lower_bound(m_filteredRows.begin(), m_filteredRows.end(), last);
    shiftFilteredRows(pos, difference);

    for (int row = first; row <= last; ++row)
        insertFilteredRow(row);
    applySort();
}

void QnSortingProxyModelPrivate::handleSourceRowsRemoved(
    const QModelIndex &parent,
    int first,
    int last)
{
    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid())
        return;

    /**
     * We have to just decrease all indicies after first by (last - first + 1) value.
     * All other work will be done in applyFilter func
     */
    const int difference = -(last - first + 1);
    const auto pos = std::upper_bound(m_filteredRows.begin(), m_filteredRows.end(), last);
    shiftFilteredRows(pos, difference);
    for (int row = first; row <= last; ++row)
        removeFilteredRow(first);

    applySort();
}

void QnSortingProxyModelPrivate::handleSourceRowsMoved(
    const QModelIndex &parent,
    int start,
    int end,
    const QModelIndex &destination,
    int row)
{
    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    NX_ASSERT(!destination.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid() || destination.isValid())
        return;

    // TODO: #ynikitenkov support handle of moving items
    NX_ASSERT(false, "QnSortingProxyModel does not handle moves yet");
}

void QnSortingProxyModelPrivate::handleSourceDataChanged(
    const QModelIndex &topLeft,
    const QModelIndex &bottomRight,
    const QVector<int> &roles)
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
        const int targetRow = m_rowsMapping.indexOf(row);
        if (targetRow == -1)
        {
            NX_ASSERT(false, "Invalid target index");
            continue;
        }

        const auto targetIndex = q->index(targetRow);
        emit q->dataChanged(targetIndex, targetIndex, roles);
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

void QnSortingProxyModel::setSortingPred(const SortingPredicate& pred)
{
    Q_D(QnSortingProxyModel);
    d->setSortingPred(pred);
}

void QnSortingProxyModel::setFilteringPred(const FilteringPredicate& pred)
{
    Q_D(QnSortingProxyModel);
    d->setFilteringPred(pred);
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
