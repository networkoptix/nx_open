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

    int sourceRowsCount() const;

    int filterRole() const;
    void setFilterRole(int value);

    int filterCaseSensitivity() const;
    void setFilterCaseSensitivity(int value);

    QString filterWildcard() const;
    void setFilterWildcard(const QString& value);

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

    void updateSourceRowsCount();

private:
    void insertSourceRow(int sourceRow);

    void removeSourceRow(int sourceRow);

    bool isFilteredOut(int sourceRow) const;

    bool isFilteredByWildcard(int sourceRow) const;

    using RowsList = QList<int>;
    int indexToInsert(int sourceRow, const RowsList& list);

    void shiftMappedRows(int fromSourceRow, int difference);

private:
    using LessPred = std::function<bool (int left, int right)>;

    const LessPred m_lessPred;
    RowsList m_mapped;
    QnSortFilterListModel::RolesSet m_triggeringRoles;

    int m_filterRole = -1;
    Qt::CaseSensitivity m_filterCaseSensiticity = Qt::CaseSensitive;
    QString m_filterWildcard;

    int m_sourceRowsCount = 0;
};

QnSortFilterListModelPrivate::QnSortFilterListModelPrivate(QnSortFilterListModel* parent):
    base_type(),
    q_ptr(parent),
    m_lessPred(
        [this](int left, int right)
        {
            Q_Q(QnSortFilterListModel);
            const auto model = q->sourceModel();
            return (model
                ? q->lessThan(model->index(left, 0), model->index(right, 0))
                : left < right);
        })
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

    auto sortedMapped = m_mapped;
    std::sort(sortedMapped.begin(), sortedMapped.end(), m_lessPred);

    RowsList forRemove;
    RowsList forAdd;

    for (int sourceRow = 0; sourceRow != sourceSize; ++sourceRow)
    {
        const int currentIndex = m_mapped.indexOf(sourceRow);
        const bool shouldBeFilteredOut = isFilteredOut(sourceRow);
        const bool currentFilteredOut = (currentIndex == -1);

        if (shouldBeFilteredOut != currentFilteredOut)
        {
            if (shouldBeFilteredOut)
                forRemove.append(sourceRow);
            else
                forAdd.append(sourceRow);

            continue;
        }
        else if (currentIndex == -1) //< It is new row.
        {
            forAdd.append(sourceRow);
            continue;
        }

        const int newIndex = indexToInsert(sourceRow, sortedMapped);
        if (newIndex == currentIndex)
            continue; //< Row is in right place.

        Q_Q(QnSortFilterListModel);

        const auto moveIndex = newIndex + (currentIndex < newIndex ? 1 : 0);
        q->beginMoveRows(QModelIndex(), currentIndex, currentIndex, QModelIndex(), moveIndex);
        m_mapped.move(currentIndex, newIndex);
        q->endMoveRows();
    }

    for (const auto rowForAdd: forAdd)
        insertSourceRow(rowForAdd);

    for (const auto rowForRemove: forRemove)
        removeSourceRow(rowForRemove);
}

int QnSortFilterListModelPrivate::rowCount() const
{
    return m_mapped.size();
}

int QnSortFilterListModelPrivate::sourceRowsCount() const
{
    return m_sourceRowsCount;
}

int QnSortFilterListModelPrivate::filterRole() const
{
    return m_filterRole;
}

void QnSortFilterListModelPrivate::setFilterRole(int value)
{
    if (m_filterRole == value)
        return;

    m_filterRole = value;
    refresh();
}

int QnSortFilterListModelPrivate::filterCaseSensitivity() const
{
    return m_filterCaseSensiticity;
}

void QnSortFilterListModelPrivate::setFilterCaseSensitivity(int value)
{
    if (m_filterCaseSensiticity == value)
        return;

    m_filterCaseSensiticity = static_cast<Qt::CaseSensitivity>(value);
    refresh();
}

QString QnSortFilterListModelPrivate::filterWildcard() const
{
    return m_filterWildcard;
}

void QnSortFilterListModelPrivate::setFilterWildcard(const QString& value)
{
    if (m_filterWildcard == value)
        return;

    m_filterWildcard = value;
    refresh();
}

bool QnSortFilterListModelPrivate::isFilteredOut(int sourceRow) const
{
    Q_Q(const QnSortFilterListModel);
    return isFilteredByWildcard(sourceRow) || !q->filterAcceptsRow(sourceRow, QModelIndex());
}

bool QnSortFilterListModelPrivate::isFilteredByWildcard(int sourceRow) const
{
    Q_Q(const QnSortFilterListModel);
    const auto sourceModel = q->sourceModel();
    if (!sourceModel || m_filterWildcard.isEmpty() || m_filterRole < 0)
        return false;

    const auto fieldData = sourceModel->index(sourceRow, 0).data(m_filterRole).toString();
    return !fieldData.contains(m_filterWildcard, m_filterCaseSensiticity);
}

int QnSortFilterListModelPrivate::indexToInsert(int sourceRow, const RowsList& mapped)
{
    const auto it = std::lower_bound(mapped.begin(), mapped.end(), sourceRow, m_lessPred);
    return std::distance(mapped.begin(), it);
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
    const auto index = indexToInsert(sourceRow, m_mapped);
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
    updateSourceRowsCount();

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
    updateSourceRowsCount();

    NX_ASSERT(!parent.isValid(), "QnSortFilterProxyModel works only with flat lists models");
    if (parent.isValid())
        return;

    const auto kRemoveDifference = -(last - first + 1);
    shiftMappedRows(last + 1, kRemoveDifference);
}

void QnSortFilterListModelPrivate::handleSourceRowsMoved(
    const QModelIndex& parent,
    int /* start */,
    int /* end */,
    const QModelIndex& destination,
    int /* row */)
{
    // TODO: #ynikitenkov support handling of moving items
    NX_ASSERT(false, "QnSortFilterListModel does not handle moves yet");
    NX_ASSERT(!parent.isValid() && !destination.isValid(),
        "QnSortFilterListModel works only with flat lists models");
    if (parent.isValid() || destination.isValid())
        return;

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

    const auto targetRoles = roles.toList().toSet();
    if (m_triggeringRoles.isEmpty()
        || targetRoles.contains(m_filterRole)
        || m_triggeringRoles.intersects(targetRoles))
    {
        refresh();
    }
}

void QnSortFilterListModelPrivate::handleResetSourceModel()
{
    resetTargetModel();
    updateSourceRowsCount();
    refresh();
}

void QnSortFilterListModelPrivate::updateSourceRowsCount()
{
    Q_Q(QnSortFilterListModel);
    const auto source = q->sourceModel();
    const auto newCount = source ? source->rowCount() : 0;
    if (m_sourceRowsCount == newCount)
        return;

    m_sourceRowsCount = newCount;
    emit q->sourceRowsCountChanged();
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

    d->updateSourceRowsCount();
    // TODO: #ynikitenkov Make filling of model with data in reset model mode.
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

int QnSortFilterListModel::sourceRowsCount() const
{
    Q_D(const QnSortFilterListModel);
    return d->sourceRowsCount();
}

int QnSortFilterListModel::filterRole() const
{
    Q_D(const QnSortFilterListModel);
    return d->filterRole();
}

void QnSortFilterListModel::setFilterRole(int value)
{
    Q_D(QnSortFilterListModel);
    d->setFilterRole(value);
}

int QnSortFilterListModel::filterCaseSensitivity() const
{
    Q_D(const QnSortFilterListModel);
    return d->filterCaseSensitivity();
}

void QnSortFilterListModel::setFilterCaseSensitivity(int value)
{
    Q_D(QnSortFilterListModel);
    d->setFilterCaseSensitivity(value);
}

QString QnSortFilterListModel::filterWildcard() const
{
    Q_D(const QnSortFilterListModel);
    return d->filterWildcard();
}

void QnSortFilterListModel::setFilterWildcard(const QString& value)
{
    Q_D(QnSortFilterListModel);
    d->setFilterWildcard(value);
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

QModelIndex QnSortFilterListModel::parent(const QModelIndex& /*child*/) const
{
    return QModelIndex(); //< For list models parent is always empty.
}

int QnSortFilterListModel::rowCount(const QModelIndex& /*parent*/) const
{
    Q_D(const QnSortFilterListModel);
    return d->rowCount();
}

int QnSortFilterListModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QHash<int, QByteArray> QnSortFilterListModel::roleNames() const
{
    const auto model = sourceModel();
    return (model ? model->roleNames() : QHash<int, QByteArray>());
}

bool QnSortFilterListModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    Q_D(const QnSortFilterListModel);
    auto model = sourceModel();
    if (!model || row < 0 || count < 0 || row + count > d->m_mapped.size())
        return false;

    // This is slightly changed implementation of QSortFilterProxyModel::removeRows.

    if (count == 1)
        return model->removeRow(d->m_mapped[row]);

    QVector<int> rows;
    rows.reserve(count);

    for (int i = row, end = row + count; i < end; ++i)
        rows << d->m_mapped[i];

    std::sort(rows.begin(), rows.end());

    int pos = rows.count() - 1;
    while (pos >= 0)
    {
        const int sourceEnd = rows[pos--];
        int sourceStart = sourceEnd;

        while ((pos >= 0) && (rows[pos] == sourceStart - 1))
        {
            --sourceStart;
            --pos;
        }

        if (!model->removeRows(sourceStart, sourceEnd - sourceStart + 1))
            return false;
    }

    return true;
}
