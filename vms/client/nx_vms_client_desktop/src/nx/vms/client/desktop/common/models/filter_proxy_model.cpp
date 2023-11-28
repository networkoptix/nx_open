// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filter_proxy_model.h"

#include <algorithm>
#include <memory>
#include <optional>

#include <QtCore/QPointer>

#include <nx/utils/debug_helpers/model_operation.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

namespace {

void incrementRowIndex(int* indexPtr, int delta)
{
    *indexPtr = (*indexPtr < 0) ? (*indexPtr - delta) : (*indexPtr + delta);
}

void decrementRowIndex(int* indexPtr, int delta)
{
    incrementRowIndex(indexPtr, -delta);
}

} // namespace

class FilterProxyModel::Private: public QObject
{
    FilterProxyModel* const q;

public:
    struct Mapping
    {
        const Private* const d;
        QPersistentModelIndex sourceParent;
        QVector<int> sourceToProxy;
        QVector<int> proxyToSource;

        ~Mapping() { d->m_mappingCheck.remove(this); }
    };

    struct RowRange
    {
        QModelIndex parent;
        int first = 0;
        int last = -1;
        bool isValid() const { return last >= first; }
        int count() const { return last - first + 1; }
    };

    using MappingPtr = std::shared_ptr<Mapping>;
    using Operation = nx::utils::ModelOperation;

    const IsRowAccepted noFilter = [](int, const QModelIndex&) { return true; };
    IsRowAccepted isRowAccepted = noFilter;

    bool recalculateFilterAfterLongMove = true;
    bool debugChecksEnabled = ini().developerMode;

public:
    Private(FilterProxyModel* q): q(q) {}
    void setSourceModel(QAbstractItemModel* value);

    MappingPtr ensureMapping(const QModelIndex& sourceParent) const;
    bool isValidMapping(Mapping* mapping) const;
    void invalidateMapping();
    void tidyMapping(); //< Remove source parents that became invalid.

    std::optional<QModelIndex> mapFromSource(const QModelIndex& sourceIndex) const;

    void checkMapping(const MappingPtr& mapping) const;
    void checkMapping(Mapping* mapping) const;

private:
    MappingPtr createMapping(const QModelIndex& sourceParent) const;
    bool isRecursivelyFilteredOut(const QModelIndex& sourceIndex) const;

    RowRange sourceRangeToProxy(
        const QModelIndex& sourceParent, int sourceFirst, int sourceLast);

    void recalculateFilter(const MappingPtr& mapping, int sourceFirst, int sourceCount);

    static int insertionIndex(const MappingPtr& mapping, int sourceRow);

    void sourceModelAboutToBeReset();
    void sourceModelReset();
    void sourceRowsAboutToBeInserted(const QModelIndex& sourceParent, int first, int last);
    void sourceRowsInserted(const QModelIndex& sourceParent, int first, int last);
    void sourceRowsAboutToBeRemoved(const QModelIndex& sourceParent, int first, int last);
    void sourceRowsRemoved(const QModelIndex& sourceParent, int first, int last);
    void sourceRowsAboutToBeMoved(const QModelIndex& sourceParent, int sourceFirst,
        int sourceLast, const QModelIndex& destinationParent, int destinationRow);
    void sourceRowsMoved(const QModelIndex& sourceParent, int sourceFirst,
        int sourceLast, const QModelIndex& destinationParent, int destinationRow);
    void sourceColumnsAboutToBeInserted(const QModelIndex& sourceParent, int first, int last);
    void sourceColumnsInserted();
    void sourceColumnsAboutToBeRemoved(const QModelIndex& sourceParent, int first, int last);
    void sourceColumnsRemoved();
    void sourceColumnsAboutToBeMoved(const QModelIndex& sourceParent, int sourceFirst,
        int sourceLast, const QModelIndex& destinationParent, int destinationColumn);
    void sourceColumnsMoved();
    void sourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex>& sourceParents,
        QAbstractItemModel::LayoutChangeHint hint);
    void sourceLayoutChanged(const QList<QPersistentModelIndex>& sourceParents,
        QAbstractItemModel::LayoutChangeHint hint);
    void sourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
        const QVector<int>& roles);
    void sourceHeaderDataChanged(Qt::Orientation orientation, int first, int last);

private:
    mutable QSet<Mapping*> m_mappingCheck; //< Valid mapping pointers are stored here.
    mutable QHash<QPersistentModelIndex /*sourceParent*/, MappingPtr /*childrenMapping*/> m_mapping;
    nx::utils::ScopedConnections m_modelConnections;
    QList<QPersistentModelIndex> m_layoutChangingParents;
    QList<std::pair<QModelIndex /*proxy*/, QPersistentModelIndex /*source*/>> m_changingIndexes;
    Operation m_operation;
    RowRange m_removedRange;
    bool m_moveAroundFilteredOutRows = false;
};

// ------------------------------------------------------------------------------------------------
// FilterProxyModel

FilterProxyModel::FilterProxyModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

FilterProxyModel::~FilterProxyModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

QModelIndex FilterProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    return d->mapFromSource(sourceIndex).value_or(QModelIndex());
}

QModelIndex FilterProxyModel::mapToSource(const QModelIndex& index) const
{
    const auto mapping = static_cast<Private::Mapping*>(index.internalPointer());
    if (!index.isValid()
        || !NX_ASSERT(sourceModel())
        || !NX_ASSERT(index.model() == this)
        || !NX_ASSERT(d->isValidMapping(mapping)) //< After this check we may access the mapping.
        || !NX_ASSERT(index.row() < mapping->proxyToSource.size())
        || !NX_ASSERT(index.column() < sourceModel()->columnCount(mapping->sourceParent)))
    {
        return {};
    }

    if (d->debugChecksEnabled)
        d->checkMapping(mapping);

    const int sourceRow = mapping->proxyToSource[index.row()];
    return NX_ASSERT(sourceRow >= 0 && sourceRow < sourceModel()->rowCount(mapping->sourceParent))
        ? sourceModel()->index(sourceRow, index.column(), mapping->sourceParent)
        : QModelIndex();
}

QVariant FilterProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // FilterProxyModel does not change the order of columns. Therefore, for a horizontal header,
    // it is allowed to directly transmit the section number to the source model. And for an empty
    // source model, this is necessary, because any index will be invalid.
    const int sourceSection =
        orientation == Qt::Vertical ? mapToSource(index(section, 0)).row() : section;

    return sourceModel()->headerData(sourceSection, orientation, role);
}

int FilterProxyModel::rowCount(const QModelIndex& parent) const
{
    if ((!sourceModel() && !parent.isValid()) || !NX_ASSERT(checkIndex(parent)))
        return 0;

    const auto sourceParent = mapToSource(parent);
    if (parent.isValid() != sourceParent.isValid())
        return 0;

    if (sourceModel()->rowCount(sourceParent) == 0)
        return 0; //< No need to create a mapping if there's no children.

    const auto mapping = d->ensureMapping(sourceParent);
    return mapping->proxyToSource.size();
}

int FilterProxyModel::columnCount(const QModelIndex& parent) const
{
    if (!sourceModel() && !parent.isValid())
        return 0;

    const auto sourceParent = mapToSource(parent);
    return parent.isValid() == sourceParent.isValid()
        ? sourceModel()->columnCount(sourceParent)
        : 0;
}

QModelIndex FilterProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    const auto sourceParent = mapToSource(parent);
    const auto mapping = d->ensureMapping(sourceParent);
    return createIndex(row, column, mapping.get());
}

QModelIndex FilterProxyModel::parent(const QModelIndex& index) const
{
    if (!index.isValid() || !NX_ASSERT(checkIndex(index, CheckIndexOption::DoNotUseParent)))
        return {};

    const auto mapping = static_cast<Private::Mapping*>(index.internalPointer());
    return NX_ASSERT(d->isValidMapping(mapping))
        ? mapFromSource(mapping->sourceParent)
        : QModelIndex();
}

bool FilterProxyModel::hasChildren(const QModelIndex& parent) const
{
    // Bypass QAbstractProxyModel implementation.
    return QAbstractItemModel::hasChildren(parent);
}

void FilterProxyModel::setSourceModel(QAbstractItemModel* value)
{
    d->setSourceModel(value);
}

FilterProxyModel::IsRowAccepted FilterProxyModel::filter() const
{
    return d->isRowAccepted;
}

void FilterProxyModel::setFilter(IsRowAccepted value)
{
    d->isRowAccepted = value ? value : d->noFilter;
    invalidateFilter();
}

void FilterProxyModel::invalidateFilter()
{
    const ScopedReset reset(this);
    d->invalidateMapping();
}

bool FilterProxyModel::recalculateFilterAfterLongMove() const
{
    return d->recalculateFilterAfterLongMove;
}

void FilterProxyModel::setRecalculateFilterAfterLongMove(bool value)
{
    d->recalculateFilterAfterLongMove = value;
}

bool FilterProxyModel::debugChecksEnabled() const
{
    return d->debugChecksEnabled;
}

void FilterProxyModel::setDebugChecksEnabled(bool value)
{
    d->debugChecksEnabled = value;
}

bool FilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    return d->isRowAccepted(sourceRow, sourceParent);
}

// ------------------------------------------------------------------------------------------------
// FilterProxyModel::Private

void FilterProxyModel::Private::setSourceModel(QAbstractItemModel* value)
{
    if (q->sourceModel() == value)
        return;

    m_modelConnections.reset();

    q->base_type::setSourceModel(value);

    if (!value)
        return;

    m_modelConnections << connect(value, &QAbstractItemModel::modelAboutToBeReset,
        this, &Private::sourceModelAboutToBeReset);

    m_modelConnections << connect(value, &QAbstractItemModel::modelReset,
        this, &Private::sourceModelReset);

    m_modelConnections << connect(value, &QAbstractItemModel::rowsAboutToBeInserted,
        this, &Private::sourceRowsAboutToBeInserted);

    m_modelConnections << connect(value, &QAbstractItemModel::rowsInserted,
        this, &Private::sourceRowsInserted);

    m_modelConnections << connect(value, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &Private::sourceRowsAboutToBeRemoved);

    m_modelConnections << connect(value, &QAbstractItemModel::rowsRemoved,
        this, &Private::sourceRowsRemoved);

    m_modelConnections << connect(value, &QAbstractItemModel::rowsAboutToBeMoved,
        this, &Private::sourceRowsAboutToBeMoved);

    m_modelConnections << connect(value, &QAbstractItemModel::rowsMoved,
        this, &Private::sourceRowsMoved);

    m_modelConnections << connect(value, &QAbstractItemModel::columnsAboutToBeInserted,
        this, &Private::sourceColumnsAboutToBeInserted);

    m_modelConnections << connect(value, &QAbstractItemModel::columnsInserted,
        this, &Private::sourceColumnsInserted);

    m_modelConnections << connect(value, &QAbstractItemModel::columnsAboutToBeRemoved,
        this, &Private::sourceColumnsAboutToBeRemoved);

    m_modelConnections << connect(value, &QAbstractItemModel::columnsRemoved,
        this, &Private::sourceColumnsRemoved);

    m_modelConnections << connect(value, &QAbstractItemModel::columnsAboutToBeMoved,
        this, &Private::sourceColumnsAboutToBeMoved);

    m_modelConnections << connect(value, &QAbstractItemModel::columnsMoved,
        this, &Private::sourceColumnsAboutToBeMoved);

    m_modelConnections << connect(value, &QAbstractItemModel::layoutAboutToBeChanged,
        this, &Private::sourceLayoutAboutToBeChanged);

    m_modelConnections << connect(value, &QAbstractItemModel::layoutChanged,
        this, &Private::sourceLayoutChanged);

    m_modelConnections << connect(value, &QAbstractItemModel::dataChanged,
        this, &Private::sourceDataChanged);

    m_modelConnections << connect(value, &QAbstractItemModel::headerDataChanged,
        this, &Private::sourceHeaderDataChanged);
}

std::optional<QModelIndex> FilterProxyModel::Private::mapFromSource(
    const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid())
        return QModelIndex();

    if (!NX_ASSERT(q->sourceModel() && q->sourceModel()->checkIndex(sourceIndex)))
        return std::nullopt;

    if (isRecursivelyFilteredOut(sourceIndex))
        return std::nullopt;

    const auto mapping = ensureMapping(sourceIndex.parent());
    const int row = NX_ASSERT(sourceIndex.row() < mapping->sourceToProxy.size())
        ? mapping->sourceToProxy[sourceIndex.row()]
        : -1;

    if (row < 0)
        return std::nullopt;

    return q->createIndex(row, sourceIndex.column(), mapping.get());
}

FilterProxyModel::Private::MappingPtr FilterProxyModel::Private::ensureMapping(
    const QModelIndex& sourceParent) const
{
    if (!m_mapping.contains(sourceParent))
        return m_mapping[sourceParent] = createMapping(sourceParent);

    return m_mapping.value(sourceParent);
}

FilterProxyModel::Private::MappingPtr FilterProxyModel::Private::createMapping(
    const QModelIndex& sourceParent) const
{
    if (!NX_ASSERT(q->sourceModel()))
        return {};

    const MappingPtr mapping(new Mapping{this, sourceParent});
    m_mappingCheck.insert(mapping.get());

    const int sourceRowCount = q->sourceModel()->rowCount(sourceParent);

    if (!m_operation.inProgress(Operation::rowInsert))
        NX_ASSERT(sourceRowCount > 0, "Should not create a mapping for an empty source parent");

    for (int sourceRow = 0; sourceRow < sourceRowCount; ++sourceRow)
    {
        if (q->filterAcceptsRow(sourceRow, sourceParent))
        {
            mapping->sourceToProxy.push_back(mapping->proxyToSource.size());
            mapping->proxyToSource.push_back(sourceRow);
        }
        else
        {
            mapping->sourceToProxy.push_back(-1 - mapping->proxyToSource.size());
        }
    }

    if (debugChecksEnabled)
        checkMapping(mapping);

    return mapping;
}

bool FilterProxyModel::Private::isRecursivelyFilteredOut(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid())
        return false;

    const auto sourceParent = sourceIndex.parent();
    if (!m_mapping.contains(sourceParent) && isRecursivelyFilteredOut(sourceParent))
        return true;

    const auto mapping = ensureMapping(sourceParent);
    return !NX_ASSERT(sourceIndex.row() < mapping->sourceToProxy.size())
        || mapping->sourceToProxy[sourceIndex.row()] < 0;
}

void FilterProxyModel::Private::invalidateMapping()
{
    m_mapping = {};
    if (!NX_ASSERT(m_mappingCheck.isEmpty()))
        m_mappingCheck = {};
}

bool FilterProxyModel::Private::isValidMapping(Mapping* mapping) const
{
    return mapping && m_mappingCheck.contains(mapping);
}

void FilterProxyModel::Private::tidyMapping()
{
    // Remove no longer valid source parents, but keep initially invalid QPersistentModelIndex{}.
    // Initially invalid QPersistentModelIndex{} has a null pointer for its internal data,
    // while an index that became invalid due to a model change has a not null internal data
    // pointer with an invalid model index stored there.

    // We have no access to internal data pointers, but can look up the intially invalid index
    // this way:
    const auto initiallyInvalidIndexIt = m_mapping.constFind(QPersistentModelIndex{});

    for (auto it = m_mapping.begin(); it != m_mapping.end(); )
    {
        if (it == initiallyInvalidIndexIt || it.key().isValid())
            ++it;
        else
            it = m_mapping.erase(it);
    }
}

FilterProxyModel::Private::RowRange FilterProxyModel::Private::sourceRangeToProxy(
    const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    const std::optional<QModelIndex> parent = mapFromSource(sourceParent);
    if (!parent)
        return {};

    if (!NX_ASSERT(sourceFirst <= sourceLast))
        std::swap(sourceFirst, sourceLast);

    const auto mapping = ensureMapping(sourceParent);

    const int first = insertionIndex(mapping, sourceFirst);
    int last = insertionIndex(mapping, sourceLast);
    if (mapping->sourceToProxy[sourceLast] < 0)
        --last;

    return {*parent, first, last};
}

void FilterProxyModel::Private::sourceModelAboutToBeReset()
{
    NX_VERBOSE(q, "Source model about to be reset");
    NX_CRITICAL(m_operation.begin(Operation::modelReset));
    q->beginResetModel();
    invalidateMapping();
}

void FilterProxyModel::Private::sourceModelReset()
{
    NX_VERBOSE(q, "Source model reset");
    NX_CRITICAL(m_operation.end(Operation::modelReset));
    q->endResetModel();
}

void FilterProxyModel::Private::sourceRowsAboutToBeInserted(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source rows about to be inserted; sourceParent=%1, first=%2, last=%3",
        sourceParent, first, last);

    NX_CRITICAL(m_operation.canBegin(Operation::rowInsert));
    const std::optional<QModelIndex> parent = mapFromSource(sourceParent);
    if (!parent)
        return;

    m_operation.begin(Operation::rowInsert);
    ensureMapping(sourceParent);
}

void FilterProxyModel::Private::sourceRowsInserted(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source rows inserted");
    if (!m_operation.end(Operation::rowInsert))
        return;

    const auto mapping = ensureMapping(sourceParent);
    const int nextRow = insertionIndex(mapping, first);

    if (!NX_ASSERT(nextRow >= 0))
    {
        invalidateMapping();
        return;
    }

    const int count = last - first + 1;

    // Ensure that the mapping is valid when rowsAboutToBeInserted signal is emitted.
    // At this point the mapping shows all inserted rows filtered out.
    mapping->sourceToProxy.insert(mapping->sourceToProxy.begin() + first, count, -1 - nextRow);

    for (int row = nextRow; row < mapping->proxyToSource.size(); ++row)
        mapping->proxyToSource[row] += count;

    if (debugChecksEnabled)
        checkMapping(mapping);

    QVector<int> proxyToSource;
    QVector<int> sourceToProxy;

    // Calculate which of inserted rows should be filtered in.
    for (int sourceRow = first; sourceRow <= last; ++sourceRow)
    {
        if (q->filterAcceptsRow(sourceRow, sourceParent))
        {
            sourceToProxy.push_back(nextRow + proxyToSource.size());
            proxyToSource.push_back(sourceRow);
        }
        else
        {
            sourceToProxy.push_back(-1 - nextRow - proxyToSource.size());
        }
    }

    const int insertedCount = proxyToSource.size();
    if (insertedCount == 0)
        return;

    const ScopedInsertRows insertRows(
        q, q->mapFromSource(sourceParent), nextRow, nextRow + insertedCount - 1);

    mapping->proxyToSource.insert(nextRow, insertedCount, -1);
    std::copy(proxyToSource.cbegin(), proxyToSource.cend(), mapping->proxyToSource.begin() + nextRow);
    std::copy(sourceToProxy.cbegin(), sourceToProxy.cend(), mapping->sourceToProxy.begin() + first);

    for (int sourceRow = last + 1; sourceRow < mapping->sourceToProxy.size(); ++sourceRow)
        incrementRowIndex(&mapping->sourceToProxy[sourceRow], insertedCount);

    if (debugChecksEnabled)
        checkMapping(mapping);
}

void FilterProxyModel::Private::sourceRowsAboutToBeRemoved(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source rows about to be removed; sourceParent=%1, first=%2, last=%3",
        sourceParent, first, last);

    NX_CRITICAL(m_operation.canBegin(Operation::rowRemove));
    const std::optional<QModelIndex> parent = mapFromSource(sourceParent);
    if (!parent)
        return;

    m_removedRange = sourceRangeToProxy(sourceParent, first, last);
    m_operation.begin(Operation::rowRemove);

    if (m_removedRange.parent.isValid())
        NX_ASSERT(isValidMapping((Mapping*) m_removedRange.parent.internalPointer()));

    if (m_removedRange.isValid())
        q->beginRemoveRows(m_removedRange.parent, m_removedRange.first, m_removedRange.last);
}

void FilterProxyModel::Private::sourceRowsRemoved(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source rows removed");
    if (!m_operation.end(Operation::rowRemove))
        return;

    const auto mapping = ensureMapping(sourceParent);
    const int count = last - first + 1;

    mapping->sourceToProxy.remove(first, count);

    if (m_removedRange.isValid())
    {
        mapping->proxyToSource.remove(m_removedRange.first,
            m_removedRange.last - m_removedRange.first + 1);

        const int removedCount = m_removedRange.count();

        for (int sourceRow = first; sourceRow < mapping->sourceToProxy.size(); ++sourceRow)
            decrementRowIndex(&mapping->sourceToProxy[sourceRow], removedCount);
    }

    for (int row = m_removedRange.first; row < mapping->proxyToSource.size(); ++row)
        mapping->proxyToSource[row] -= count;

    tidyMapping();
    if (debugChecksEnabled)
        checkMapping(mapping);

    if (m_removedRange.parent.isValid())
        NX_ASSERT(isValidMapping((Mapping*) m_removedRange.parent.internalPointer()));

    if (m_removedRange.isValid())
        q->endRemoveRows();
}

void FilterProxyModel::Private::sourceRowsAboutToBeMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationRow)
{
    NX_VERBOSE(q, "Source rows about to be moved; sourceParent=%1, first=%2, last=%3, "
        "destinationParent=%4, destinationRow=%5", sourceParent, sourceFirst, sourceLast,
        destinationParent, destinationRow);

    NX_CRITICAL(m_operation.canBegin(Operation::rowMove));
    m_removedRange = {};
    m_moveAroundFilteredOutRows = false;

    const std::optional<QModelIndex> parentFrom = mapFromSource(sourceParent);
    const std::optional<QModelIndex> parentTo = sourceParent != destinationParent
        ? mapFromSource(destinationParent)
        : parentFrom;

    if (parentTo)
    {
        m_operation.begin(Operation::rowInsert);
        ensureMapping(destinationParent);
    }

    if (parentFrom)
    {
        m_operation.begin(Operation::rowRemove);
        m_removedRange = sourceRangeToProxy(sourceParent, sourceFirst, sourceLast);

        if (m_removedRange.isValid())
        {
            if (parentTo)
            {
                const auto mapping = ensureMapping(destinationParent);
                const int proxyRow = insertionIndex(mapping, destinationRow);

                m_moveAroundFilteredOutRows = *parentFrom == *parentTo
                    && (proxyRow == m_removedRange.first || proxyRow == m_removedRange.last + 1);

                if (!m_moveAroundFilteredOutRows && !NX_ASSERT(q->beginMoveRows(
                    *parentFrom, m_removedRange.first, m_removedRange.last, *parentTo, proxyRow)))
                {
                    invalidateMapping();
                    m_operation.end(Operation::rowMove);
                }
            }
            else
            {
                q->beginRemoveRows(m_removedRange.parent, m_removedRange.first,
                    m_removedRange.last);
            }
        }
    }
}

void FilterProxyModel::Private::sourceRowsMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationRow)
{
    NX_VERBOSE(q, "Source rows moved");

    switch (m_operation & Operation::rowMove)
    {
        case Operation::rowInsert:
        {
            NX_VERBOSE(q, "Handle move as insertion");
            sourceRowsInserted(destinationParent, destinationRow,
                destinationRow + sourceLast - sourceFirst);
            break;
        }

        case Operation::rowRemove:
        {
            NX_VERBOSE(q, "Handle move as removal");
            sourceRowsRemoved(sourceParent, sourceFirst, sourceLast);
            break;
        }

        case Operation::rowMove:
        {
            m_operation.end(Operation::rowMove);
            const auto sourceMapping = ensureMapping(sourceParent);

            if (sourceParent == destinationParent)
            {
                // Move within the same parent. No filter recalculation.

                const auto [beginIndex, newBeginIndex, endIndex] = destinationRow < sourceFirst
                    ? std::make_tuple(destinationRow, sourceFirst, sourceLast + 1)
                    : std::make_tuple(sourceFirst, sourceLast + 1, destinationRow);

                int nextRow = insertionIndex(sourceMapping, beginIndex);

                std::rotate(sourceMapping->sourceToProxy.begin() + beginIndex,
                    sourceMapping->sourceToProxy.begin() + newBeginIndex,
                    sourceMapping->sourceToProxy.begin() + endIndex);

                // Rebuild affected part of the mapping.
                for (int sourceRow = beginIndex; sourceRow < endIndex; ++sourceRow)
                {
                    if (sourceMapping->sourceToProxy[sourceRow] >= 0)
                    {
                        sourceMapping->sourceToProxy[sourceRow] = nextRow;
                        sourceMapping->proxyToSource[nextRow++] = sourceRow;
                    }
                    else
                    {
                        sourceMapping->sourceToProxy[sourceRow] = -1 - nextRow;
                    }
                }

                if (debugChecksEnabled)
                    checkMapping(sourceMapping);

                if (!m_moveAroundFilteredOutRows && m_removedRange.isValid())
                    q->endMoveRows();
            }
            else
            {
                // Move between different parents. Filter recalculation after move.

                const auto destinationMapping = ensureMapping(destinationParent);
                int nextRow = insertionIndex(destinationMapping, destinationRow);
                if (!NX_ASSERT(nextRow >= 0))
                {
                    invalidateMapping();
                    if (m_removedRange.isValid())
                        q->endMoveRows();
                    return;
                }

                const int count = sourceLast - sourceFirst + 1;
                const int movedCount = m_removedRange.count();

                destinationMapping->sourceToProxy.insert(
                    destinationMapping->sourceToProxy.begin() + destinationRow,
                    count,
                    -1 - nextRow);

                if (m_removedRange.isValid())
                {
                    destinationMapping->proxyToSource.insert(
                        destinationMapping->proxyToSource.begin() + nextRow,
                        movedCount,
                        -1);
                }

                for (int i = 0; i < count; ++i)
                {
                    if (sourceMapping->sourceToProxy[sourceFirst + i] >= 0)
                    {
                        destinationMapping->sourceToProxy[destinationRow + i] = nextRow;
                        destinationMapping->proxyToSource[nextRow++] = destinationRow + i;
                    }
                    else
                    {
                        destinationMapping->sourceToProxy[destinationRow + i] = -1 - nextRow;
                    }
                }

                for (int row = nextRow; row < destinationMapping->proxyToSource.size(); ++row)
                    destinationMapping->proxyToSource[row] += count;

                for (int sourceRow = destinationRow + count;
                    sourceRow < destinationMapping->sourceToProxy.size();
                    ++sourceRow)
                {
                    incrementRowIndex(&destinationMapping->sourceToProxy[sourceRow], movedCount);
                }

                sourceMapping->sourceToProxy.remove(sourceFirst, count);

                if (m_removedRange.isValid())
                {
                    sourceMapping->proxyToSource.remove(m_removedRange.first,
                        m_removedRange.last - m_removedRange.first + 1);

                    for (int sourceRow = sourceFirst;
                        sourceRow < sourceMapping->sourceToProxy.size();
                        ++sourceRow)
                    {
                        decrementRowIndex(&sourceMapping->sourceToProxy[sourceRow], movedCount);
                    }
                }

                for (int row = m_removedRange.first;
                    row < sourceMapping->proxyToSource.size();
                    ++row)
                {
                    sourceMapping->proxyToSource[row] -= count;
                }

                if (debugChecksEnabled)
                {
                    checkMapping(sourceMapping);
                    checkMapping(destinationMapping);
                }

                if (m_removedRange.isValid())
                    q->endMoveRows();

                if (recalculateFilterAfterLongMove)
                    recalculateFilter(destinationMapping, destinationRow, count);
            }

            break;
        }

        default:
            break;
    }
}

void FilterProxyModel::Private::sourceColumnsAboutToBeInserted(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source columns about to be inserted; sourceParent=%1, first=%2, last=%3",
        sourceParent, first, last);

    NX_CRITICAL(m_operation.canBegin(Operation::columnInsert));
    const std::optional<QModelIndex> parent = mapFromSource(sourceParent);
    if (!parent)
        return;

    m_operation.begin(Operation::columnInsert);
    q->beginInsertColumns(*parent, first, last);
}

void FilterProxyModel::Private::sourceColumnsInserted()
{
    NX_VERBOSE(q, "Source columns inserted");
    if (m_operation.end(Operation::columnInsert))
        q->endInsertColumns();
}

void FilterProxyModel::Private::sourceColumnsAboutToBeRemoved(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source columns about to be removed; sourceParent=%1, first=%2, last=%3",
        sourceParent, first, last);

    NX_CRITICAL(m_operation.canBegin(Operation::columnRemove));
    const std::optional<QModelIndex> parent = mapFromSource(sourceParent);
    if (!parent)
        return;

    m_operation.begin(Operation::columnRemove);
    q->beginRemoveColumns(*parent, first, last);
}

void FilterProxyModel::Private::sourceColumnsRemoved()
{
    NX_VERBOSE(q, "Source columns removed");
    if (m_operation.end(Operation::columnRemove))
        q->endRemoveColumns();
}

void FilterProxyModel::Private::sourceColumnsAboutToBeMoved(const QModelIndex& sourceParent,
    int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationColumn)
{
    NX_VERBOSE(q, "Source columns about to be moved; sourceParent=%1, first=%2, last=%3, "
        "destinationParent=%4, destinationColumn=%5", sourceParent, sourceFirst, sourceLast,
        destinationParent, destinationColumn);

    NX_CRITICAL(m_operation.canBegin(Operation::columnMove));

    const std::optional<QModelIndex> parentFrom = mapFromSource(sourceParent);
    const std::optional<QModelIndex> parentTo = sourceParent != destinationParent
        ? mapFromSource(destinationParent)
        : parentFrom;

    if (parentFrom)
        m_operation.begin(Operation::columnRemove);
    if (parentTo)
        m_operation.begin(Operation::columnInsert);

    switch (m_operation & Operation::columnMove)
    {
        case Operation::columnMove:
            q->beginMoveColumns(*parentFrom, sourceFirst, sourceLast, *parentTo, destinationColumn);
            break;

        case Operation::columnRemove:
            q->beginRemoveColumns(*parentFrom, sourceFirst, sourceLast);
            break;

        case Operation::columnInsert:
            q->beginInsertColumns(*parentTo, destinationColumn,
                destinationColumn + (sourceLast - sourceFirst + 1));
            break;

        default:
            break;
    }
}

void FilterProxyModel::Private::sourceColumnsMoved()
{
    NX_VERBOSE(q, "Source columns moved");

    const auto operation = m_operation & Operation::columnMove;
    m_operation.end(Operation::columnMove);

    switch (operation)
    {
        case Operation::columnMove:
            q->endMoveColumns();
            break;

        case Operation::columnRemove:
            NX_VERBOSE(q, "Handle move as removal");
            tidyMapping();
            q->endRemoveColumns();
            break;

        case Operation::columnInsert:
            NX_VERBOSE(q, "Handle move as insertion");
            q->endInsertColumns();
            break;

        default:
            break;
    }
}

void FilterProxyModel::Private::sourceLayoutAboutToBeChanged(
    const QList<QPersistentModelIndex>& sourceParents, QAbstractItemModel::LayoutChangeHint hint)
{
    NX_VERBOSE(q, "Source layout about to be changed, hint=%1, parents=%2", hint, sourceParents);
    NX_CRITICAL(m_operation.canBegin(Operation::layoutChange));

    m_changingIndexes.clear();
    m_layoutChangingParents.clear();

    if (!sourceParents.empty())
    {
        for (const auto& sourceParent: sourceParents)
        {
            if (const std::optional<QModelIndex> parent = mapFromSource(sourceParent))
                m_layoutChangingParents.push_back(*parent);
        }

        if (m_layoutChangingParents.empty())
            return;
    }

    m_operation.begin(Operation::layoutChange);
    emit q->layoutAboutToBeChanged(m_layoutChangingParents, hint);

    for (const auto& index: q->persistentIndexList())
    {
        const auto source = q->mapToSource(index);
        m_changingIndexes.push_back({index, source});
    }
}

void FilterProxyModel::Private::sourceLayoutChanged(
    const QList<QPersistentModelIndex>& sourceParents, QAbstractItemModel::LayoutChangeHint hint)
{
    NX_VERBOSE(q, "Source layout changed, hint=%1, parents=%2", hint, sourceParents);

    if (m_operation == Operation::none || !NX_ASSERT(m_operation.end(Operation::layoutChange)))
        return;

    invalidateMapping();

    for (const auto changing: m_changingIndexes)
    {
        const auto newProxyIndex = q->mapFromSource(changing.second);
        q->changePersistentIndex(changing.first, newProxyIndex);
    }

    m_changingIndexes.clear();

    emit q->layoutChanged(m_layoutChangingParents, hint);
    m_layoutChangingParents.clear();
}

void FilterProxyModel::Private::sourceDataChanged(
    const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
{
    NX_ASSERT(m_operation == Operation::none);

    if (!isRecursivelyFilteredOut(topLeft.parent())
        && NX_ASSERT(topLeft.isValid() && bottomRight.isValid()
            && topLeft.parent() == bottomRight.parent()))
    {
        const auto range = sourceRangeToProxy(topLeft.parent(), topLeft.row(), bottomRight.row());
        if (range.isValid())
        {
            const auto mapping = ensureMapping(topLeft.parent());
            emit q->dataChanged(
                q->createIndex(range.first, topLeft.column(), mapping.get()),
                q->createIndex(range.last, bottomRight.column(), mapping.get()),
                roles);
        }
    }
}

void FilterProxyModel::Private::sourceHeaderDataChanged(
    Qt::Orientation orientation, int first, int last)
{
    if (orientation == Qt::Horizontal)
    {
        emit q->headerDataChanged(Qt::Horizontal, first, last);
    }
    else
    {
        const auto range = sourceRangeToProxy({}, first, last);
        if (range.isValid())
            emit q->headerDataChanged(Qt::Vertical, range.first, range.last);
    }
}

int FilterProxyModel::Private::insertionIndex(const MappingPtr& mapping, int sourceRow)
{
    if (!NX_ASSERT(mapping && sourceRow >= 0 && sourceRow <= mapping->sourceToProxy.size()))
        return -1;

    if (sourceRow == mapping->sourceToProxy.size())
        return mapping->proxyToSource.size();

    const int proxyRow = mapping->sourceToProxy[sourceRow];
    return proxyRow < 0 ? (-1 - proxyRow) : proxyRow;
}

void FilterProxyModel::Private::checkMapping(
    const FilterProxyModel::Private::MappingPtr& mapping) const
{
    checkMapping(mapping.get());
}

void FilterProxyModel::Private::checkMapping(
    FilterProxyModel::Private::Mapping* mapping) const
{
    if (!NX_ASSERT(mapping && q->sourceModel()))
        return;

    if (!NX_ASSERT(isValidMapping(mapping)))
        return;

    int nextProxyIndex = 0;
    for (int sourceIndex = 0; sourceIndex < mapping->sourceToProxy.size(); ++sourceIndex)
    {
        const int proxyIndex = mapping->sourceToProxy[sourceIndex];
        if (proxyIndex < 0)
        {
            NX_ASSERT(proxyIndex == -1 - nextProxyIndex);
        }
        else
        {
            NX_ASSERT(proxyIndex == nextProxyIndex);
            NX_ASSERT(proxyIndex < mapping->proxyToSource.size());
            NX_ASSERT(mapping->proxyToSource[proxyIndex] == sourceIndex);
            ++nextProxyIndex;
        }
    }

    NX_ASSERT(nextProxyIndex == mapping->proxyToSource.size());
    NX_ASSERT(mapping->sourceToProxy.size() == q->sourceModel()->rowCount(mapping->sourceParent));
}

void FilterProxyModel::Private::recalculateFilter(
    const MappingPtr& mapping, int sourceFirst, int sourceCount)
{
    if (!NX_ASSERT(mapping && sourceCount))
        return;

    if (!NX_ASSERT(sourceFirst + sourceCount <= mapping->sourceToProxy.size()))
        return;

    QVector<int> sourceToProxy;
    QVector<int> proxyToSource;

    const int nextRow = insertionIndex(mapping, sourceFirst);
    const int sourceEnd = sourceFirst + sourceCount;

    for (int sourceRow = sourceFirst, row = nextRow; sourceRow < sourceEnd; ++sourceRow)
    {
        if (q->filterAcceptsRow(sourceRow, mapping->sourceParent))
        {
            sourceToProxy.push_back(row++);
            proxyToSource.push_back(sourceRow);
        }
        else
        {
            sourceToProxy.push_back(-1 - row);
        }
    }

    if (std::equal(sourceToProxy.cbegin(), sourceToProxy.cend(),
        mapping->sourceToProxy.cbegin() + sourceFirst))
    {
        NX_VERBOSE(q, "Filtered state didn't change.");
        return;
    }

    NX_VERBOSE(q, "Filtered state changed, updating layout.");

    const QPersistentModelIndex parent = q->mapFromSource(mapping->sourceParent);
    emit q->layoutAboutToBeChanged({parent});

    std::copy(sourceToProxy.cbegin(), sourceToProxy.cend(),
        mapping->sourceToProxy.begin() + sourceFirst);

    const int oldProxyCount = insertionIndex(mapping, sourceEnd) - nextRow;
    const int delta = proxyToSource.size() - oldProxyCount;

    if (delta > 0)
        mapping->proxyToSource.insert(nextRow, delta, -1);
    else if (delta < 0)
        mapping->proxyToSource.remove(nextRow, -delta);

    std::copy(proxyToSource.cbegin(), proxyToSource.cend(),
        mapping->proxyToSource.begin() + nextRow);

    if (delta != 0)
    {
        for (int sourceRow = sourceEnd; sourceRow < mapping->sourceToProxy.size(); ++sourceRow)
            incrementRowIndex(&mapping->sourceToProxy[sourceRow], delta);
    }

    if (debugChecksEnabled)
        checkMapping(mapping);

    emit q->layoutChanged({parent});
}

} // namespace nx::vms::client::desktop
