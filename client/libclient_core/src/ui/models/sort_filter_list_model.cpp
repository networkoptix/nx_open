#include "sort_filter_list_model.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <utils/common/connective.h>

namespace {

struct RemoveOperationData
{
    int startIndex;
    int offset;
};

} // unnamed namespace

//////////////////////////////////////////////////////////////////////////////////////////////////
class QnSortFilterListModelPrivate : public Connective<QObject>
{
    using base_type = Connective<QObject>;

    Q_DECLARE_PUBLIC(QnSortFilterListModel)
    QnSortFilterListModel *q_ptr;

public:
    QnSortFilterListModelPrivate(QnSortFilterListModel* parent);

    void setModel(QAbstractListModel* model);
    QAbstractListModel* model() const;

    void setTriggeringRoles(const QnSortFilterListModel::RolesList& roles);

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

    void handleSourceRowsAboutToBeRemoved(
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

    void removeSourceRow(int mappedIndex);

    bool isFilteredOut(int sourceRow) const;

    int indexToInsert(int sourceRow);

    void shiftMappedRows(int fromSourceRow, int difference);

private:
    using RowsList = QList<int>;
    using RolesSet = QSet<int>;
    using RemoveOperationDataPtr = QScopedPointer<RemoveOperationData>;

    QPointer<QAbstractListModel> m_model;
    RowsList m_mapped;
    RolesSet m_triggeringRoles;
    RemoveOperationDataPtr m_removeOperationData;
};

QnSortFilterListModelPrivate::QnSortFilterListModelPrivate(QnSortFilterListModel* parent):
    base_type(),
    q_ptr(parent)
{
}

void QnSortFilterListModelPrivate::setModel(QAbstractListModel* model)
{
    if (m_model == model)
        return;

    if (m_model)
    {
        disconnect(m_model);

        Q_Q(QnSortFilterListModel);
        q->beginResetModel();
        m_mapped.clear();
        q->endResetModel();
    }

    m_model = model;
    if (!m_model)
        return;

    const auto resetShiftingOperation = [this]() { m_removeOperationData.reset(); };

    connect(m_model, &QAbstractListModel::rowsInserted,
        this, &QnSortFilterListModelPrivate::handleSourceRowsInserted);


    connect(m_model, &QAbstractListModel::rowsAboutToBeRemoved,
        this, &QnSortFilterListModelPrivate::handleSourceRowsAboutToBeRemoved);
    connect(m_model, &QAbstractListModel::rowsRemoved, this, resetShiftingOperation);


    connect(m_model, &QAbstractListModel::rowsMoved,
        this, &QnSortFilterListModelPrivate::handleSourceRowsMoved);
    connect(m_model, &QAbstractListModel::dataChanged,
        this, &QnSortFilterListModelPrivate::handleSourceDataChanged);

    invalidate();
}

QAbstractListModel* QnSortFilterListModelPrivate::model() const
{
    return m_model;
}

void QnSortFilterListModelPrivate::setTriggeringRoles(const QnSortFilterListModel::RolesList& roles)
{
    m_triggeringRoles = roles.toSet();
}

QModelIndex QnSortFilterListModelPrivate::sourceIndexForTargetRow(int row) const
{
    if (!m_model || (row < 0) || (row >= m_mapped.size()))
        return QModelIndex();

    if (m_removeOperationData && (row >= m_removeOperationData->startIndex))
        row += m_removeOperationData->offset;
    const auto sourceRow = m_mapped[row];
    return m_model->index(sourceRow);
}

void QnSortFilterListModelPrivate::invalidate()
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
            {
                /**
                 * Index will never be -1 here because it is not filtered out currently.
                 * removeSourceRow checks this anyway.
                 */
                NX_ASSERT(currentIndex != -1, "Index can't be -1 here!");
                removeSourceRow(currentIndex);
            }
            else
                insertSourceRow(sourceRow); // Here row is placed in right position

            continue;
        }
        else if (currentIndex == -1) //< It is new row
        {
            insertSourceRow(sourceRow);
            continue;
        }

        const int newIndex = indexToInsert(sourceRow);
        if (newIndex == currentIndex)
            continue; //< Row is in right place

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

int QnSortFilterListModelPrivate::indexToInsert(
    int sourceRow)
{
    const auto it = std::lower_bound(m_mapped.begin(), m_mapped.end(), sourceRow,
        [this](int left, int right)
        {
            Q_Q(QnSortFilterListModel);
            return q->lessThan(m_model->index(left), m_model->index(right));
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

void QnSortFilterListModelPrivate::removeSourceRow(int mappedIndex)
{
    if (mappedIndex == -1)
        return; //< Row is filtered out

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
    };
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

    NX_ASSERT(m_removeOperationData.isNull(), "Overlapped remove rows opertaion");

    static const auto kRemoveDifference = -1;
    for (int row = first; row <= last; ++row)
    {        
        const int indicesOffset = row - first + 1;
        m_removeOperationData.reset(new RemoveOperationData({first, indicesOffset}));

        const int mappedIndex = m_mapped.indexOf(first);
        shiftMappedRows(first + 1, kRemoveDifference);
        removeSourceRow(mappedIndex);
    }
}

void QnSortFilterListModelPrivate::handleSourceRowsMoved(
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
    NX_ASSERT(false, "QnSortFilterListModel does not handle moves yet");
}

void QnSortFilterListModelPrivate::handleSourceDataChanged(
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
        NX_ASSERT(false, "QnSortFilterListModel supports only flat lists models");
        return;
    }

    Q_Q(QnSortFilterListModel);
    for(int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {
        const int targetRow = m_mapped.indexOf(row);
        if (targetRow == -1)
            continue;

        const auto targetIndex = q->index(targetRow);
        emit q->dataChanged(targetIndex, targetIndex, roles);
    }

    if (m_triggeringRoles.isEmpty() ||
        !RolesSet(m_triggeringRoles).intersect(roles.toList().toSet()).isEmpty())
    {
        invalidate();
    }
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

void QnSortFilterListModel::setSourceModel(QAbstractListModel* model)
{
    Q_D(QnSortFilterListModel);
    d->setModel(model);
}

QAbstractListModel* QnSortFilterListModel::sourceModel() const
{
    Q_D(const QnSortFilterListModel);
    return d->model();
}

void QnSortFilterListModel::setTriggeringRoles(const RolesList& roles)
{
    Q_D(QnSortFilterListModel);
    d->setTriggeringRoles(roles);
}

void QnSortFilterListModel::forceUpdate()
{
    Q_D(QnSortFilterListModel);
    d->invalidate();
}

bool QnSortFilterListModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    Q_D(const QnSortFilterListModel);
    return (sourceLeft.row() < sourceRight.row());
}

bool QnSortFilterListModel::filterAcceptsRow(
    int /* sourceRow */,
    const QModelIndex& /* sourceParent */) const
{
    Q_D(const QnSortFilterListModel);
    return true;
}

int QnSortFilterListModel::rowCount(const QModelIndex& /* parent */) const
{
    Q_D(const QnSortFilterListModel);
    return d->rowCount();
}

QVariant QnSortFilterListModel::data(const QModelIndex& index, int role) const
{
    Q_D(const QnSortFilterListModel);
    const auto modelIndex = d->sourceIndexForTargetRow(index.row());
    return modelIndex.data(role);
}

QHash<int, QByteArray> QnSortFilterListModel::roleNames() const
{
    Q_D(const QnSortFilterListModel);
    const auto model = d->model();
    return (model ? model->roleNames() : QHash<int, QByteArray>());
}
