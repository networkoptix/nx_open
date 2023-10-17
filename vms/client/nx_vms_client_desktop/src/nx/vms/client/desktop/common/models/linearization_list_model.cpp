// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "linearization_list_model.h"

#include <algorithm>
#include <memory>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QSet>
#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/application_context.h>

template<typename Container, typename ToString>
QString itemsToString(const Container& items, ToString itemToString)
{
    if (items.empty())
        return {};

    QString result = itemToString(*items.cbegin());
    for (auto iter = items.cbegin() + 1; iter != items.cend(); ++iter)
        result += ", " + itemToString(*iter);

    return result;
}

QString toString(const QModelIndex& index)
{
    if (!index.isValid())
        return "!";

    return index.parent().isValid()
        ? QString("%1:%2").arg(toString(index.parent()), QString::number(index.row()))
        : QString::number(index.row());
}

namespace nx::vms::client::desktop {

namespace {

static constexpr int kNoRole = -1;

/**
 * If `nestedChild` is a direct child of `parent`, returns `nestedChild`. If an indirect child,
 * for example in a hierarchy like `parent-child1-child2-child3-nestedChild`, returns `child1`.
 */
QModelIndex indexInParent(const QModelIndex& parent, const QModelIndex& nestedChild)
{
    for (auto currentChild = nestedChild; currentChild.isValid();)
    {
        const auto currentParent = currentChild.parent();
        if (currentParent == parent)
            return currentChild;

        currentChild = currentParent;
    }

    return {};
}

inline bool within(int value, int first, int last)
{
    return value >= first && value <= last;
}

} // namespace

class LinearizationListModel::Private: public QObject
{
    LinearizationListModel* const q;

public:
    Private(LinearizationListModel* q): q(q) {}

    QAbstractItemModel* sourceModel() const { return m_sourceModel.data(); }
    void setSourceModel(QAbstractItemModel* value);

    QModelIndex sourceRoot() const;
    void setSourceRoot(const QModelIndex& sourceIndex);

    QString autoExpandRoleName() const;
    void setAutoExpandRoleName(const QString& value);

    QModelIndex mapToSource(const QModelIndex& index) const;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const;

    int level(const QModelIndex& index) const;
    int rowCount() const;

    bool isSourceExpanded(const QModelIndex& sourceIndex) const;
    bool setExpanded(const QModelIndex& index, bool value, bool recursively);
    void setExpanded(
        const QModelIndex& index,
        std::function<bool(const QModelIndex&)> shouldExpand);
    void setExpanded(QJSValue shouldExpand, const QJSValue& topLevelRows);
    bool setSourceExpanded(const QModelIndex& sourceIndex, bool value);

    QModelIndexList expandedSourceIndices() const;
    void setExpandedSourceIndices(const QModelIndexList& sourceIndices);

    void expandAll();
    void collapseAll();

private:
    void sourceModelAboutToBeReset();
    void sourceModelReset();
    void sourceColumnsAboutToBeChanged(const QModelIndex& sourceParent);
    void sourceColumnsChanged(const QModelIndex& sourceParent);
    void sourceRowsAboutToBeInserted(const QModelIndex& sourceParent, int first, int last);
    void sourceRowsInserted(const QModelIndex& sourceParent, int first, int last);
    void sourceRowsAboutToBeRemoved(const QModelIndex& sourceParent, int first, int last);
    void sourceRowsRemoved(const QModelIndex& sourceParent, int first, int last);
    void sourceRowsAboutToBeMoved(const QModelIndex& sourceParent, int first, int last,
        const QModelIndex& destinationParent, int destination);
    void sourceRowsMoved(const QModelIndex& sourceParent, int first, int last,
        const QModelIndex& destinationParent, int destination);
    void sourceDataChanged(const QModelIndex& sourceTopLeft, const QModelIndex& sourceBottomRight,
        const QVector<int>& roles = QVector<int>());
    void sourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex>& parents,
        QAbstractItemModel::LayoutChangeHint hint);
    void sourceLayoutChanged(const QList<QPersistentModelIndex>& parents,
        QAbstractItemModel::LayoutChangeHint hint);

private:
    struct Node;
    using NodePtr = std::shared_ptr<Node>;
    using NodeList = std::vector<NodePtr>;

    struct Node
    {
        std::weak_ptr<Node> parent;
        NodeList children;
        int sourceRow = 0;

        static constexpr int kRootNodeRow = -1;
        int row = kRootNodeRow;

        Node() = default;
        Node(const NodePtr& parent, int sourceRow, int row):
            parent(parent), sourceRow(sourceRow), row(row) {}

        int childCount() const { return (int) children.size(); }
    };

    void reset();
    void insertChildren(NodePtr node, int position, int count);
    void removeChildren(NodePtr node, int first = 0, int count = -1);
    std::pair<int, int> moveChildren(NodePtr sourceNode, int first, int count,
        NodePtr destinationNode, int destination); //< Returns new linear indices {first, count}.
    void rebuildNode(NodePtr node);
    void internalRebuildNode(NodePtr node, int& nextRow);

    void expandRecursively(NodePtr node, bool force);
    void collapseRecursively(const QModelIndex& sourceIndex);

    NodePtr getNode(const QModelIndex& sourceIndex) const;

    QModelIndex sourceIndex(const NodePtr& node) const;
    QModelIndex index(const NodePtr& node) const;
    int nextRow(const NodePtr& after) const;

    void setAutoExpandRole(int value);
    int calculateAutoExpandRole() const;

    void autoExpand(const QModelIndex& sourceParent, int first, int count);
    void markAutoExpandedIndicesRecursively(const QModelIndex& sourceParent, int first, int count);

    bool debugCheckIntegrity() const;
    bool debugCheckLinearIntegrity() const;
    bool debugCheckTreeIntegrity(const NodePtr& from, int& currentLinearIndex) const;

    QString nodePath(const NodePtr& node) const { return toString(sourceIndex(node)); }

public:
    friend size_t qHash(const NodePtr& node, size_t seed) { return ::qHash(node.get(), seed); }

private:
    QPointer<QAbstractItemModel> m_sourceModel;
    QPersistentModelIndex m_sourceRoot;
    QSet<QPersistentModelIndex> m_expandedSourceIndices;

    std::vector<PersistentIndexPair> m_changingIndices;
    NodeList m_layoutChangingNodes;

    QString m_autoExpandRoleName;
    int m_autoExpandRole = kNoRole;

    // Layout change internal data.
    NodeList m_rebuiltNodes;
    QList<QPersistentModelIndex> m_visibleSourceIndices;

    // Rows insertion/removal/move operation.
    bool m_operationInProgress = false;

    // Whether a source move operation changes item levels, but not linear positions.
    //               +-- A         +-- A
    //               |             |   |
    // For example,  +-- B   ==>   |   +-- B     Linear A,B,C stays linear A,B,C
    //               |             |             levels change from 0,0,0 to 0,1,0
    //               +-- C         +-- C
    bool m_moveWithoutReorder = false;

    // Move operation parent nodes.
    NodePtr m_moveSourceParent;
    NodePtr m_moveDestinationParent;

    // An operation that requires reset due to source root change.
    bool m_rootChanging = false;

    // Expanded tree.
    NodePtr m_rootNode;
    NodeList m_nodeList;
};

// ------------------------------------------------------------------------------------------------
// LinearizationListModel

LinearizationListModel::LinearizationListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

LinearizationListModel::~LinearizationListModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

QAbstractItemModel* LinearizationListModel::sourceModel() const
{
    return d->sourceModel();
}

void LinearizationListModel::setSourceModel(QAbstractItemModel* value)
{
    d->setSourceModel(value);
}

QModelIndex LinearizationListModel::sourceRoot() const
{
    return d->sourceRoot();
}

void LinearizationListModel::setSourceRoot(const QModelIndex& sourceIndex)
{
    d->setSourceRoot(sourceIndex);
}

QString LinearizationListModel::autoExpandRoleName() const
{
    return d->autoExpandRoleName();
}

void LinearizationListModel::setAutoExpandRoleName(const QString& value)
{
    d->setAutoExpandRoleName(value);
}

QModelIndex LinearizationListModel::mapToSource(const QModelIndex& index) const
{
    return d->mapToSource(index);
}

QModelIndex LinearizationListModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    return d->mapFromSource(sourceIndex);
}

int LinearizationListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->rowCount();
}

QVariant LinearizationListModel::data(const QModelIndex& index, int role) const
{
    const auto sourceIndex = d->mapToSource(index);
    if (!sourceIndex.isValid())
        return {};

    switch (role)
    {
        case LevelRole:
            return d->level(index);

        case HasChildrenRole:
            return d->sourceModel()->hasChildren(sourceIndex);

        case ExpandedRole:
            return d->isSourceExpanded(sourceIndex)
                && NX_ASSERT(d->sourceModel()->hasChildren(sourceIndex));

        default:
            return d->sourceModel()->data(sourceIndex, role);
    }
}

bool LinearizationListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    switch (role)
    {
        case LevelRole:
        case HasChildrenRole:
            return false;

        case ExpandedRole:
            return d->setExpanded(index, value.toBool(), /*recursively*/ false);

        default:
            return d->sourceModel()->setData(d->mapToSource(index), value, role);
    }
}

void LinearizationListModel::expandAll()
{
    d->expandAll();
}

void LinearizationListModel::collapseAll()
{
    d->collapseAll();
}

bool LinearizationListModel::setExpanded(const QModelIndex& index, bool value, bool recursively)
{
    return d->setExpanded(index, value, recursively);
}

bool LinearizationListModel::setSourceExpanded(const QModelIndex& sourceIndex, bool value)
{
    return d->setSourceExpanded(sourceIndex, value);
}

bool LinearizationListModel::isSourceExpanded(const QModelIndex& sourceIndex) const
{
    return d->isSourceExpanded(sourceIndex);
}

QModelIndexList LinearizationListModel::expandedState() const
{
    return d->expandedSourceIndices();
}

void LinearizationListModel::setExpandedState(const QModelIndexList& sourceIndices)
{
    d->setExpandedSourceIndices(sourceIndices);
}

void LinearizationListModel::setExpanded(
    const QJSValue& expandCallback,
    const QJSValue& topLevelRows)
{
    d->setExpanded(expandCallback, topLevelRows);
}

void LinearizationListModel::ensureVisible(const QModelIndexList& sourceIndices)
{
    auto expandedSourceIndices = d->expandedSourceIndices();
    for (const auto& index: sourceIndices)
    {
        for (auto parent = index.parent(); parent.isValid(); parent = parent.parent())
            expandedSourceIndices.push_back(parent); //< Duplicates will be filtered later.
    }

    d->setExpandedSourceIndices(expandedSourceIndices);
}

QHash<int, QByteArray> LinearizationListModel::roleNames() const
{
    static const QHash<int, QByteArray> kRoles{
        {LevelRole, "level"},
        {HasChildrenRole, "hasChildren"},
        {ExpandedRole, "expanded"}};

    auto result = base_type::roleNames();
    result.insert(kRoles);
    return result;
}

void LinearizationListModel::registerQmlType()
{
    qmlRegisterType<LinearizationListModel>("nx.vms.client.desktop", 1, 0, "LinearizationListModel");
}

// ------------------------------------------------------------------------------------------------
// LinearizationListModel::Private

void LinearizationListModel::Private::setSourceModel(QAbstractItemModel* value)
{
    if (m_sourceModel == value)
        return;

    NX_CRITICAL(!m_operationInProgress);

    const auto notifier = nx::utils::makeScopeGuard(
        [this]() { emit q->sourceModelChanged(LinearizationListModel::QPrivateSignal()); });

    ScopedReset scopedReset(q);
    QScopedValueRollback progressRollback(m_operationInProgress, true);

    if (m_sourceModel)
        m_sourceModel->disconnect(this);

    m_sourceRoot = QModelIndex();
    m_sourceModel = value;
    m_autoExpandRole = calculateAutoExpandRole();

    reset();
    m_operationInProgress = false;

    if (!m_sourceModel)
        return;

    connect(m_sourceModel, &QAbstractItemModel::modelAboutToBeReset,
        this, &Private::sourceModelAboutToBeReset);

    connect(m_sourceModel, &QAbstractItemModel::modelReset,
        this, &Private::sourceModelReset);

    connect(m_sourceModel, &QAbstractItemModel::rowsAboutToBeInserted,
        this, &Private::sourceRowsAboutToBeInserted);

    connect(m_sourceModel, &QAbstractItemModel::rowsInserted,
        this, &Private::sourceRowsInserted);

    connect(m_sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, &Private::sourceRowsAboutToBeRemoved);

    connect(m_sourceModel, &QAbstractItemModel::rowsRemoved,
        this, &Private::sourceRowsRemoved);

    connect(m_sourceModel, &QAbstractItemModel::rowsAboutToBeMoved,
        this, &Private::sourceRowsAboutToBeMoved);

    connect(m_sourceModel, &QAbstractItemModel::rowsMoved,
        this, &Private::sourceRowsMoved);

    connect(m_sourceModel, &QAbstractItemModel::dataChanged,
        this, &Private::sourceDataChanged);

    connect(m_sourceModel, &QAbstractItemModel::columnsAboutToBeInserted,
        this, &Private::sourceColumnsAboutToBeChanged);

    connect(m_sourceModel, &QAbstractItemModel::columnsInserted,
        this, &Private::sourceColumnsChanged);

    connect(m_sourceModel, &QAbstractItemModel::columnsAboutToBeRemoved,
        this, &Private::sourceColumnsAboutToBeChanged);

    connect(m_sourceModel, &QAbstractItemModel::columnsRemoved,
        this, &Private::sourceColumnsChanged);

    connect(m_sourceModel, &QAbstractItemModel::columnsAboutToBeMoved,
        this, &Private::sourceColumnsAboutToBeChanged);

    connect(m_sourceModel, &QAbstractItemModel::columnsMoved,
        this, &Private::sourceColumnsChanged);

    connect(m_sourceModel, &QAbstractItemModel::layoutAboutToBeChanged,
        this, &Private::sourceLayoutAboutToBeChanged);

    connect(m_sourceModel, &QAbstractItemModel::layoutChanged,
        this, &Private::sourceLayoutChanged);
}

QModelIndex LinearizationListModel::Private::sourceRoot() const
{
    return m_sourceRoot;
}

void LinearizationListModel::Private::setSourceRoot(const QModelIndex& sourceIndex)
{
    if (m_sourceModel && !NX_ASSERT(m_sourceModel->checkIndex(sourceIndex)))
        return;

    const auto newSourceRoot = sourceIndex.isValid() && !NX_ASSERT(sourceIndex.column() == 0)
        ? sourceIndex.sibling(sourceIndex.row(), 0)
        : sourceIndex;

    if (m_sourceRoot == newSourceRoot)
        return;

    NX_CRITICAL(!m_operationInProgress);

    ScopedReset scopedReset(q, m_sourceModel != nullptr);
    QScopedValueRollback progressRollback(m_operationInProgress, true);
    m_sourceRoot = newSourceRoot;
    reset();
}

QString LinearizationListModel::Private::autoExpandRoleName() const
{
    return m_autoExpandRoleName;
}

void LinearizationListModel::Private::setAutoExpandRoleName(const QString& value)
{
    if (m_autoExpandRoleName == value)
        return;

    m_autoExpandRoleName = value;
    setAutoExpandRole(calculateAutoExpandRole());

    emit q->autoExpandRoleNameChanged(LinearizationListModel::QPrivateSignal());
}

void LinearizationListModel::Private::setAutoExpandRole(int value)
{
    if (m_autoExpandRole == value)
        return;

    m_autoExpandRole = value;
    if (!m_sourceModel || m_autoExpandRole == kNoRole)
        return;

    autoExpand(m_sourceRoot, 0, m_sourceModel->rowCount(m_sourceRoot));
}

int LinearizationListModel::Private::calculateAutoExpandRole() const
{
    if (!m_sourceModel || m_autoExpandRoleName.isEmpty())
        return kNoRole;

    const auto roleNames = m_sourceModel->roleNames();
    const auto iter = std::find_if(roleNames.cbegin(), roleNames.cend(),
        [this](const QString& name) { return m_autoExpandRoleName == name; });

    return (iter != roleNames.cend()) ? iter.key() : kNoRole;
}

void LinearizationListModel::Private::sourceModelAboutToBeReset()
{
    NX_VERBOSE(q, "Source model about to be reset");
    NX_CRITICAL(!m_operationInProgress);
    NX_VERBOSE(q, "Beginning to reset model");
    q->beginResetModel();
    m_operationInProgress = true;
}

void LinearizationListModel::Private::sourceModelReset()
{
    NX_VERBOSE(q, "Source model reset");
    NX_CRITICAL(m_operationInProgress);
    reset();
    m_operationInProgress = false;
    q->endResetModel();
    NX_VERBOSE(q, "Finished resetting model");
}

void LinearizationListModel::Private::sourceColumnsAboutToBeChanged(const QModelIndex& sourceParent)
{
    NX_VERBOSE(q, "Source columns about to be changed");
    NX_CRITICAL(m_sourceModel && m_sourceModel->rowCount(sourceParent) == 0,
        "Column operations on the source model are not supported");
}

void LinearizationListModel::Private::sourceColumnsChanged(const QModelIndex& sourceParent)
{
    NX_VERBOSE(q, "Source columns changed");
    NX_CRITICAL(m_sourceModel && m_sourceModel->rowCount(sourceParent) == 0,
        "Column operations on the source model are not supported");
}

void LinearizationListModel::Private::sourceRowsAboutToBeInserted(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source rows about to be inserted; sourceParent=%1, first=%2, last=%3",
        sourceParent, first, last);

    NX_CRITICAL(!m_operationInProgress && !m_rootChanging);
    const auto node = getNode(sourceParent);

    if (!node || (node != m_rootNode && node->children.empty() /*might be not expanded*/))
    {
        NX_VERBOSE(q, "Insertion into an invisible part; skipping");
        return;
    }

    if (!NX_ASSERT(first >= 0 && first <= node->childCount() && last >= first))
        return;

    const int linearPos = first < node->childCount()
        ? node->children[first]->row
        : nextRow(node);

    NX_VERBOSE(q, "Beginning to insert rows; first=%1, last=%2",
        linearPos, linearPos + (last - first));

    q->beginInsertRows({}, linearPos, linearPos + (last - first));
    m_operationInProgress = true;
}

void LinearizationListModel::Private::sourceRowsInserted(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source rows inserted");

    const auto node = getNode(sourceParent);
    const int count = last - first + 1;

    const bool insertNewChildren = m_operationInProgress && NX_ASSERT(node);
    if (insertNewChildren)
    {
        insertChildren(node, first, count);
        m_operationInProgress = false;
        q->endInsertRows();
        NX_VERBOSE(q, "Finished inserting rows");
    }

    if (node && node != m_rootNode && m_sourceModel->rowCount(sourceParent) == count)
    {
        const auto parent = index(node);
        if (NX_ASSERT(parent.isValid()))
        {
            NX_VERBOSE(q, "HasChildrenRole changed for node at row=%1", node->row);
            emit q->dataChanged(parent, parent, {HasChildrenRole});
        }

        // LinearizationListModel concept is that nodes without children can not be expanded.
        // Therefore if new items are inserted into a previously empty parent, we must attempt
        // to auto-expand that parent.
        autoExpand(sourceParent.parent(), node->sourceRow, 1);
    }
    else if (insertNewChildren)
    {
        autoExpand(sourceParent, first, last - first + 1);
    }
}

void LinearizationListModel::Private::sourceRowsAboutToBeRemoved(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source rows about to be removed; sourceParent=%1, first=%2, last=%3",
        sourceParent, first, last);

    NX_CRITICAL(!m_operationInProgress && !m_rootChanging);

    if (within(indexInParent(sourceParent, m_sourceRoot).row(), first, last))
    {
        NX_VERBOSE(q, "Source root is being removed: translating to model reset");
        sourceModelAboutToBeReset();
        m_rootChanging = true;
        return;
    }

    const auto node = getNode(sourceParent);
    if (!node || (node != m_rootNode && node->children.empty()) /*might be not expanded*/)
    {
        NX_VERBOSE(q, "Removal within an invisible part; skipping");
        return;
    }

    if (!NX_ASSERT(first >= 0 && last >= first && last < node->childCount()))
        return;

    const int linearBegin = node->children[first]->row;
    const int linearEnd = nextRow(node->children[last]);

    NX_VERBOSE(q, "Beginning to remove rows; first=%1, last=%2", linearBegin, linearEnd - 1);

    q->beginRemoveRows({}, linearBegin, linearEnd - 1);
    m_operationInProgress = true;
}

void LinearizationListModel::Private::sourceRowsRemoved(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_VERBOSE(q, "Source rows removed");

    if (m_rootChanging)
    {
        m_rootChanging = false;
        sourceModelReset();
        NX_VERBOSE(q, "Source root was removed");
        return;
    }

    const auto node = getNode(sourceParent);
    if (m_operationInProgress && NX_ASSERT(node))
    {
        removeChildren(node, first, last - first + 1);
        m_operationInProgress = false;
        q->endRemoveRows();
        NX_VERBOSE(q, "Finished removing rows");
    }

    if (node && node != m_rootNode && !m_sourceModel->hasChildren(sourceParent))
    {
        const auto parent = index(node);
        if (NX_ASSERT(parent.isValid()))
        {
            const bool wasExpanded = m_expandedSourceIndices.remove(sourceParent);

            NX_VERBOSE(q, "%2 changed for node at row=%1", node->row, wasExpanded
                ? "HasChildrenRole and ExpandedRole"
                : "HasChildrenRole");

            emit q->dataChanged(parent, parent, wasExpanded
                ? QVector<int>({HasChildrenRole, ExpandedRole})
                : QVector<int>({HasChildrenRole}));
        }
    }
}

void LinearizationListModel::Private::sourceRowsAboutToBeMoved(const QModelIndex& sourceParent,
    int first, int last, const QModelIndex& destinationParent, int destination)
{
    NX_VERBOSE(q, "Source rows about to be moved; sourceParent=%1, first=%2, last=%3, "
        "destinationParent=%4, destinationRow=%5", sourceParent, first, last,
        destinationParent, destination);

    NX_CRITICAL(!m_operationInProgress && !m_rootChanging);

    if (within(indexInParent(sourceParent, m_sourceRoot).row(), first, last))
    {
        NX_VERBOSE(q, "Source root is being moved, underlying tree isn't changed; skipping");
        return;
    }

    m_moveSourceParent = getNode(sourceParent);
    m_moveDestinationParent = getNode(destinationParent);

    const bool hasSourceItems = m_moveSourceParent && !(m_moveSourceParent != m_rootNode
        && m_moveSourceParent->children.empty() /*might be not expanded*/);

    const bool destinationExpanded = m_moveDestinationParent == m_rootNode
        || isSourceExpanded(destinationParent);

    const bool hasDestination = m_moveDestinationParent && (hasSourceItems || destinationExpanded);
    if (hasDestination && !destinationExpanded)
    {
        NX_VERBOSE(q, "Expanding destination parent %1 (row %2)",
            nodePath(m_moveDestinationParent), m_moveDestinationParent->row);

        const auto childCount = m_sourceModel->rowCount(destinationParent);
        m_expandedSourceIndices.insert(destinationParent);
        const auto index = q->index(m_moveDestinationParent->row);
        emit q->dataChanged(index, index, {ExpandedRole});

        if (childCount > 0)
        {
            insertChildren(m_moveDestinationParent, 0, childCount);
            expandRecursively(m_moveDestinationParent, false);
        }
    }

    if (hasSourceItems)
    {
        if (!NX_ASSERT(first >= 0 && last >= first && last < m_moveSourceParent->childCount()))
            return;

        const int linearBegin = m_moveSourceParent->children[first]->row;
        const int linearEnd = nextRow(m_moveSourceParent->children[last]);

        if (hasDestination)
        {
            // Moving.
            const int linearPos = destination < m_moveDestinationParent->childCount()
                ? m_moveDestinationParent->children[destination]->row
                : nextRow(m_moveDestinationParent);

            NX_VERBOSE(q, "Beginning to move rows; first=%1, last=%2, destination=%3",
                linearBegin, linearEnd - 1, linearPos);

            if ((linearPos == linearBegin || linearPos == linearEnd)
                && m_moveSourceParent != m_moveDestinationParent)
            {
                m_operationInProgress = true;
                m_moveWithoutReorder = true;
            }
            else
            {
                m_operationInProgress = NX_ASSERT(
                    q->beginMoveRows({}, linearBegin, linearEnd - 1, {}, linearPos));
            }
        }
        else
        {
            NX_VERBOSE(q, "Moving to an invisible part: removal");

            NX_VERBOSE(q, "Beginning to remove rows; first=%1, last=%2",
                linearBegin, linearEnd - 1);

            // Removal.
            q->beginRemoveRows({}, linearBegin, linearEnd - 1);
            m_operationInProgress = true;
        }
    }
    else if (hasDestination)
    {
        NX_VERBOSE(q, "Moving from an invisible part: insertion");

        // Insertion.
        const int linearPos = destination < m_moveDestinationParent->childCount()
            ? m_moveDestinationParent->children[destination]->row
            : nextRow(m_moveDestinationParent);

        NX_VERBOSE(q, "Beginning to insert rows; first=%1, last=%2",
            linearPos, linearPos + (last - first));

        q->beginInsertRows({}, linearPos, linearPos + (last - first));
        m_operationInProgress = true;
    }
    else
    {
        NX_VERBOSE(q, "Moving within an invisible part; skipping");
    }
}

void LinearizationListModel::Private::sourceRowsMoved(const QModelIndex& sourceParent,
    int first, int last, const QModelIndex& destinationParent, int destination)
{
    NX_VERBOSE(q, "Source rows moved");

    const int count = last - first + 1;

    const auto clearInternalPointers = nx::utils::makeScopeGuard(
        [this]()
        {
            m_moveSourceParent.reset();
            m_moveDestinationParent.reset();
        });

    bool insertedNewChildren = false;
    const bool withinInvisible = !m_operationInProgress;
    std::pair<int, int> moved{};

    if (m_operationInProgress)
    {
        const bool hasSourceItems = m_moveSourceParent && !(m_moveSourceParent != m_rootNode
            && m_moveSourceParent->children.empty() /*might be not expanded*/);

        const bool hasDestination = m_moveDestinationParent
            && (m_moveDestinationParent == m_rootNode
                || m_expandedSourceIndices.contains(destinationParent));

        if (hasSourceItems)
        {
            if (hasDestination)
            {
                // Moving.
                moved = moveChildren(
                    m_moveSourceParent, first, count, m_moveDestinationParent, destination);
                m_operationInProgress = false;
                if (!m_moveWithoutReorder)
                    q->endMoveRows();
                m_moveWithoutReorder = false;
                NX_VERBOSE(q, "Finished moving rows");
            }
            else
            {
                // Removal.
                removeChildren(m_moveSourceParent, first, count);
                m_operationInProgress = false;
                q->endRemoveRows();
                NX_VERBOSE(q, "Finished removing rows");
            }
        }
        else if (hasDestination)
        {
            // Insertion;
            insertChildren(m_moveDestinationParent, destination, count);
            insertedNewChildren = true;
            m_operationInProgress = false;
            q->endInsertRows();
            NX_VERBOSE(q, "Finished inserting rows");
        }
        else
        {
            NX_ASSERT(false, "Should never happen");
            m_operationInProgress = false;
        }
    }

    if (m_moveSourceParent == m_moveDestinationParent)
        return;

    if (m_moveSourceParent && m_moveSourceParent != m_rootNode
        && !m_sourceModel->hasChildren(sourceParent))
    {
        const auto parent = index(m_moveSourceParent);
        if (NX_ASSERT(parent.isValid()))
        {
            const bool wasExpanded = m_expandedSourceIndices.remove(sourceParent);

            NX_VERBOSE(q, "%2 changed for node at row=%1", m_moveSourceParent->row, wasExpanded
                ? "HasChildrenRole and ExpandedRole"
                : "HasChildrenRole");

            emit q->dataChanged(parent, parent, wasExpanded
                ? QVector<int>({HasChildrenRole, ExpandedRole})
                : QVector<int>({HasChildrenRole}));
        }
    }

    if (m_moveDestinationParent && m_moveDestinationParent != m_rootNode
        && m_sourceModel->rowCount(destinationParent) == count)
    {
        const auto parent = index(m_moveDestinationParent);
        if (NX_ASSERT(parent.isValid()))
        {
            NX_VERBOSE(q, "HasChildrenRole changed for node at row=%1",
                m_moveDestinationParent->row);
            emit q->dataChanged(parent, parent, {HasChildrenRole});
        }

        if (withinInvisible)
        {

            // LinearizationListModel concept is that nodes without children can not be expanded.
            // Therefore if new items are inserted into a previously empty parent, we must attempt
            // to auto-expand that parent.
            const auto parentOfParent = destinationParent.parent();
            if (!parentOfParent.isValid() || m_expandedSourceIndices.contains(parentOfParent))
                autoExpand(parentOfParent, m_moveDestinationParent->sourceRow, count);
        }
    }
    else if (insertedNewChildren)
    {
        autoExpand(destinationParent, destination, count);
    }

    if (moved.second)
    {
        NX_VERBOSE(q, "LevelRole changed; firstRow=%1, lastRow=%2", moved.first,
            moved.first + moved.second - 1);

        emit q->dataChanged(
            q->index(moved.first), q->index(moved.first + moved.second - 1), {LevelRole});
    }
}

void LinearizationListModel::Private::sourceDataChanged(const QModelIndex& sourceTopLeft,
    const QModelIndex& sourceBottomRight, const QVector<int>& roles)
{
    // NX_VERBOSE(q, "Source data changed; sourceTopLeft=%1, sourceBottomRight=%2",
    //     sourceTopLeft, sourceBottomRight);

    if (!sourceTopLeft.isValid() || !sourceBottomRight.isValid() || !NX_ASSERT(
        m_sourceModel->checkIndex(sourceTopLeft) && m_sourceModel->checkIndex(sourceBottomRight)))
    {
        return;
    }

    const auto topLeft = mapFromSource(sourceTopLeft);
    const auto bottomRight = mapFromSource(sourceBottomRight);

    if (!NX_ASSERT(topLeft.isValid() == bottomRight.isValid()) || !topLeft.isValid())
        return;

    emit q->dataChanged(topLeft, bottomRight, roles);
}

void LinearizationListModel::Private::sourceLayoutAboutToBeChanged(
    const QList<QPersistentModelIndex>& parents, QAbstractItemModel::LayoutChangeHint hint)
{
    NX_VERBOSE(q, "Source layout about to be changed; hint=%1, parents=[%2]", hint,
        itemsToString(parents, [](const QModelIndex& index) { return toString(index); }));

    NX_CRITICAL(hint != QAbstractItemModel::HorizontalSortHint,
        "Column operations on the source model are not supported");

    NX_CRITICAL(!m_operationInProgress && !m_rootChanging && m_layoutChangingNodes.empty());

    // Build changing nodes set.
    QSet<NodePtr> layoutChangingNodes;
    if (parents.empty())
    {
        layoutChangingNodes.insert(m_rootNode);
    }
    else
    {
        for (const auto& parent: parents)
        {
            const auto node = getNode(parent);
            if (node)
                layoutChangingNodes.insert(node);
        }
    }

    if (layoutChangingNodes.empty())
        return;

    // Filter out nodes residing within changing parents.
    for (const auto& node: layoutChangingNodes)
    {
        bool ok = true;
        for (auto parent = node->parent.lock(); ok && parent; parent = parent->parent.lock())
            ok = !layoutChangingNodes.contains(parent);

        if (ok)
            m_layoutChangingNodes.push_back(node);
    }

    if (!NX_ASSERT(!m_layoutChangingNodes.empty()))
        return;

    std::sort(m_layoutChangingNodes.begin(), m_layoutChangingNodes.end(),
        [](const NodePtr& left, const NodePtr& right) { return left->row < right->row; });

    if (!NX_ASSERT(m_visibleSourceIndices.empty()))
        m_visibleSourceIndices.clear();

    // Save affected visible source indices as persistent indices to ensure they are visible
    // after layout change if they're moved into a visible collapsed parent.
    if (hint != QAbstractItemModel::VerticalSortHint)
    {
        for (const auto& node: m_layoutChangingNodes)
        {
            for (const auto& child: node->children)
                m_visibleSourceIndices.push_back(sourceIndex(child));
        }
    }

    NX_VERBOSE(q, "Created visible row persistent source indices, count=%1",
        m_visibleSourceIndices.size());

    NX_VERBOSE(q, "Beginning to change layout; %1 nodes are to be rebuilt: [%2]",
        m_layoutChangingNodes.size(), itemsToString(m_layoutChangingNodes,
            [this](const NodePtr& node) { return nodePath(node); }));

    emit q->layoutAboutToBeChanged({}, hint);

    if (!NX_ASSERT(m_changingIndices.empty()))
        m_changingIndices.clear();

    const auto persistent = q->persistentIndexList();
    m_changingIndices.reserve(persistent.size());

    for (const auto& index: persistent)
        m_changingIndices.emplace_back(index, mapToSource(index));

    NX_VERBOSE(q, "%1 persistent indices are prepared to update: [%2]", m_changingIndices.size(),
        itemsToString(m_changingIndices,
            [](const PersistentIndexPair& pair)
            {
                return nx::format("%1 (%2)", toString(pair.first), toString(pair.second));
            }));
}

void LinearizationListModel::Private::sourceLayoutChanged(
    const QList<QPersistentModelIndex>& /*parents*/, QAbstractItemModel::LayoutChangeHint hint)
{
    NX_VERBOSE(q, "Source layout changed");

    NX_CRITICAL(hint != QAbstractItemModel::HorizontalSortHint,
        "Column operations on the source model are not supported");

    NX_CRITICAL(!m_operationInProgress && !m_rootChanging);

    if (m_layoutChangingNodes.empty())
        return;

    NX_VERBOSE(q, "Updating expanded source indices");

    // All rows that were visible should stay visible.
    for (const auto& index: m_visibleSourceIndices)
    {
        const auto parent = index.parent();
        if (parent.isValid())
            m_expandedSourceIndices.insert(parent);
    }

    // Remove any index that became invalid from expanded source index set.
    QSet<QPersistentModelIndex> toRemove;
    for (const auto& index: m_expandedSourceIndices)
    {
        if (!index.isValid())
            toRemove.insert(index);
    }

    m_expandedSourceIndices -= toRemove;

    NX_VERBOSE(q, "Rebuilding affected nodes");

    for (const auto& node: m_layoutChangingNodes)
        rebuildNode(node);

    std::vector<std::pair<int, int>> affectedRanges;
    for (const auto& node: m_layoutChangingNodes)
        affectedRanges.emplace_back(node->row, nextRow(node));

    m_layoutChangingNodes.clear();
    NX_ASSERT_HEAVY_CONDITION(debugCheckIntegrity());

    NX_VERBOSE(q, "Updating persistent indices");

    for (const auto& changed: m_changingIndices)
    {
        NX_CRITICAL(!changed.second.isValid() || changed.second.column() == 0,
            "Column operations on the source model are not supported");

        q->changePersistentIndex(changed.first, mapFromSource(changed.second));
    }

    NX_VERBOSE(q, "Updated persistent indices: [%1]", itemsToString(m_changingIndices,
        [](const PersistentIndexPair& pair)
        {
            return nx::format("%1 (%2)", toString(pair.first), toString(pair.second));
        }));

    m_changingIndices.clear();
    m_visibleSourceIndices.clear();

    emit q->layoutChanged({}, hint);

    NX_VERBOSE(q, "Finished changing layout");

    for (const auto range: std::as_const(affectedRanges))
    {
        // Emitting dataChanged for all affected nodes for all possibly changed roles.
        // This must be the fastest way to do it, instead of notifying about every actual change.

        static const QVector<int> kPossibleRoles({LevelRole, HasChildrenRole, ExpandedRole});
        emit q->dataChanged(
            q->index(std::max(range.first, 0)), //< Skip artificial root node if necessary.
            q->index(range.second - 1),
            kPossibleRoles);
    }
}

QModelIndex LinearizationListModel::Private::mapToSource(const QModelIndex& index) const
{
    if (!index.isValid())
        return m_sourceRoot;

    if (!NX_ASSERT(q->checkIndex(index)))
        return {};

    return sourceIndex(m_nodeList[index.row()]);
}

QModelIndex LinearizationListModel::Private::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid() || !m_sourceModel
        || !NX_ASSERT(m_sourceModel->checkIndex(sourceIndex)))
    {
        return {};
    }

    return index(getNode(sourceIndex));
}

int LinearizationListModel::Private::level(const QModelIndex& index) const
{
    int level = -1;
    if (!index.isValid() || !NX_ASSERT(q->checkIndex(index)))
        return level;

    for (auto node = m_nodeList[index.row()]->parent.lock();
        node != nullptr;
        node = node->parent.lock())
    {
        ++level;
    }

    return level;
}

int LinearizationListModel::Private::rowCount() const
{
    return (int) m_nodeList.size();
}

bool LinearizationListModel::Private::isSourceExpanded(const QModelIndex& sourceIndex) const
{
    return m_expandedSourceIndices.contains(sourceIndex);
}

void LinearizationListModel::Private::autoExpand(
    const QModelIndex& sourceParent, int first, int count)
{
    if (!NX_ASSERT(m_sourceModel) || m_autoExpandRole == kNoRole)
        return;

    if (!NX_ASSERT(sourceParent == m_sourceRoot || isSourceExpanded(sourceParent)))
        return;

    NX_VERBOSE(q, "Auto expand; sourceParent=%1, first=%2, count=%3", sourceParent, first, count);

    markAutoExpandedIndicesRecursively(sourceParent, first, count);

    if (auto node = getNode(sourceParent))
        expandRecursively(node, /*force*/ false);
}

void LinearizationListModel::Private::markAutoExpandedIndicesRecursively(
    const QModelIndex& sourceParent, int first, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const auto sourceIndex = m_sourceModel->index(first + i, 0, sourceParent);
        if (!NX_ASSERT(sourceIndex.isValid()))
            continue;

        const auto childCount = m_sourceModel->rowCount(sourceIndex);
        markAutoExpandedIndicesRecursively(sourceIndex, 0, childCount);

        if (sourceIndex.data(m_autoExpandRole).toBool() && childCount > 0)
            m_expandedSourceIndices.insert(sourceIndex);
    }
}

void LinearizationListModel::Private::setExpanded(
    const QModelIndex& index, std::function<bool(const QModelIndex&)> shouldExpand)
{
    if (!index.isValid())
        return;

    if (!m_sourceModel->hasChildren(index))
        return;

    if (shouldExpand(index))
        setSourceExpanded(index, true);

    for (int row = 0; row < m_sourceModel->rowCount(index); row++)
        setExpanded(m_sourceModel->index(row, 0, index), shouldExpand);
}

bool LinearizationListModel::Private::setExpanded(
    const QModelIndex& index, bool value, bool recursively)
{
    const auto sourceIndex = mapToSource(index);
    if (!sourceIndex.isValid())
        return false;

    const bool wasExpanded = isSourceExpanded(sourceIndex);

    if ((!recursively && wasExpanded == value)
        || (value && !m_sourceModel->hasChildren(sourceIndex)))
    {
        return false;
    }

    const auto node = getNode(sourceIndex);
    if (!NX_ASSERT(node))
        return false;

    if (value)
    {
        // Expand.
        NX_VERBOSE(q, "Expanding node %1 (row %2)", toString(sourceIndex), node->row);
        if (!wasExpanded && NX_ASSERT(node->children.empty()))
        {
            m_expandedSourceIndices.insert(sourceIndex);
            emit q->dataChanged(index, index, {ExpandedRole});
            insertChildren(node, 0, m_sourceModel->rowCount(sourceIndex));
        }

        expandRecursively(node, /*force*/ recursively);
    }
    else
    {
        // Collapse.
        NX_VERBOSE(q, "Collapsing node %1 (row %2)", toString(sourceIndex), node->row);
        if (!node->children.empty())
            removeChildren(node);

        if (recursively)
            collapseRecursively(sourceIndex);
        else
            m_expandedSourceIndices.remove(sourceIndex);

        if (wasExpanded)
            emit q->dataChanged(index, index, {ExpandedRole});
    }

    return true;
}

bool LinearizationListModel::Private::setSourceExpanded(const QModelIndex& sourceIndex, bool value)
{
    if (!sourceIndex.isValid() || !m_sourceModel)
        return false;

    const auto index = mapFromSource(sourceIndex);
    if (index.isValid())
        return setExpanded(index, value, /*recursively*/ false);

    if (value == m_expandedSourceIndices.contains(sourceIndex))
        return false;

    if (value)
        m_expandedSourceIndices.insert(sourceIndex);
    else
        m_expandedSourceIndices.remove(sourceIndex);

    return true;
}

void LinearizationListModel::Private::expandRecursively(NodePtr node, bool force)
{
    for (const auto& child: node->children)
    {
        const auto sourceIndex = this->sourceIndex(child);
        const bool wasExpanded = m_expandedSourceIndices.contains(sourceIndex);

        if (child->children.empty()
            && (force || wasExpanded)
            && m_sourceModel->hasChildren(sourceIndex))
        {
            if (!wasExpanded)
            {
                NX_VERBOSE(q, "Expanding node %1 (row %2)", nodePath(child), child->row);
                m_expandedSourceIndices.insert(sourceIndex);
                const auto index = q->index(child->row);
                emit q->dataChanged(index, index, {ExpandedRole});
            }

            insertChildren(child, 0, m_sourceModel->rowCount(sourceIndex));
        }

        expandRecursively(child, force);
    }
}

// Only removes source indices from m_expandedSourceIndices set.
void LinearizationListModel::Private::collapseRecursively(const QModelIndex& sourceIndex)
{
    m_expandedSourceIndices.remove(sourceIndex);

    const int rowCount = m_sourceModel->rowCount(sourceIndex);

    for (int row = 0; row < rowCount; ++row)
        collapseRecursively(m_sourceModel->index(row, 0, sourceIndex));
}

void LinearizationListModel::Private::expandAll()
{
    if (!m_sourceModel)
        return;

    NX_VERBOSE(q, "Expand all");
    expandRecursively(m_rootNode, true);
}

void LinearizationListModel::Private::collapseAll()
{
    if (!m_sourceModel)
        return;

    NX_VERBOSE(q, "Collapse all");

    for (const auto& child: m_rootNode->children)
        setExpanded(index(child), false, /*recursively*/ false);

    m_expandedSourceIndices.clear(); //< Due to this we don't need to collapse recursively.
}

QModelIndexList LinearizationListModel::Private::expandedSourceIndices() const
{
    if (!m_sourceModel)
        return {};

    QModelIndexList sourceIndices;
    sourceIndices.reserve(m_expandedSourceIndices.size());

    for (const auto& sourceIndex: m_expandedSourceIndices)
    {
        if (sourceIndex.isValid())
            sourceIndices.push_back(sourceIndex);
    }

    return sourceIndices;
}

void LinearizationListModel::Private::setExpandedSourceIndices(const QModelIndexList& sourceIndices)
{
    if (!m_sourceModel)
        return;

    QSet<QPersistentModelIndex> expandedSourceIndices;
    for (const auto& sourceIndex: sourceIndices)
    {
        // The assertion is temporarily commented out due to QnResourceTreeModel inconsistency.
        if (sourceIndex.isValid() && /*NX_ASSERT*/(m_sourceModel->checkIndex(sourceIndex)))
            expandedSourceIndices.insert(sourceIndex);
    }

    for (int row = 0; row < q->rowCount(); ++row)
    {
        const auto index = q->index(row);
        setExpanded(index, expandedSourceIndices.contains(mapToSource(index)),
            /*recursively*/ false);
    }

    m_expandedSourceIndices = expandedSourceIndices;
}

void LinearizationListModel::Private::setExpanded(
    QJSValue shouldExpand,
    const QJSValue& topLevelRows)
{
    if (!shouldExpand.isCallable())
        return;

    if (!topLevelRows.isArray())
        return;

    const int length = topLevelRows.property("length").toInt();

    for (int i = 0; i < length; ++i)
    {
        const auto row = topLevelRows.property(i).toInt();
        const auto index = m_sourceModel->index(row, 0);

        setExpanded(index,
            [&shouldExpand](const QModelIndex& index)
            {
                auto arg = appContext()->qmlEngine()->toScriptValue(index);
                return shouldExpand.call({arg}).toBool();
            });
    }
}

void LinearizationListModel::Private::reset()
{
    NX_VERBOSE(q, "Resetting data");

    m_rootNode = std::make_shared<Node>();
    m_nodeList.clear();
    m_expandedSourceIndices.clear();

    if (!m_sourceModel)
        return;

    const int count = m_sourceModel->rowCount(m_sourceRoot);
    m_rootNode->children.reserve(count);

    for (int row = 0; row < count; ++row)
    {
        auto child = std::make_shared<Node>(m_rootNode, row, row);
        m_rootNode->children.push_back(child);
        m_nodeList.push_back(child);
    }

    autoExpand(m_sourceRoot, 0, m_sourceModel->rowCount(m_sourceRoot));
}

void LinearizationListModel::Private::insertChildren(NodePtr node, int position, int count)
{
    NX_CRITICAL(node && position >= 0 && position <= node->childCount());

    NX_VERBOSE(q, "Inserting children to node at row=%1, position=%2, count=%3",
        node->row, position, count);

    const int linearPos = position < node->childCount()
        ? node->children[position]->row
        : nextRow(node);

    NodeList children(count);

    int index = 0;
    for (auto& child: children)
    {
        child.reset(new Node());
        child->parent = node;
        child->sourceRow = position + index;
        child->row = linearPos + index;
        ++index;
    }

    ScopedInsertRows scopedInsertRows(q, linearPos, linearPos + count - 1, !m_operationInProgress);

    for (auto iter = node->children.begin() + position; iter != node->children.end(); ++iter)
        (*iter)->sourceRow += count;

    node->children.insert(node->children.begin() + position, children.begin(), children.end());

    for (auto iter = m_nodeList.begin() + linearPos; iter != m_nodeList.end(); ++iter)
        (*iter)->row += count;

    m_nodeList.insert(m_nodeList.begin() + linearPos, std::make_move_iterator(children.begin()),
        std::make_move_iterator(children.end()));

    NX_ASSERT_HEAVY_CONDITION(debugCheckIntegrity());
}

void LinearizationListModel::Private::removeChildren(NodePtr node, int first, int count)
{
    if (count == 0)
        return;

    NX_CRITICAL(node && first >= 0);

    const int end = count >= 0 ? (first + count) : node->childCount();

    NX_VERBOSE(q, "Removing children from node at row=%1, first=%2, count=%3",
        node->row, first, end - first);

    NX_CRITICAL(end <= node->childCount());

    const int linearBegin = node->children[first]->row;
    const int linearEnd = nextRow(node->children[end - 1]);
    const int linearCount = linearEnd - linearBegin;

    ScopedRemoveRows scopedRemoveRows(q, linearBegin, linearEnd - 1, !m_operationInProgress);
    m_nodeList.erase(m_nodeList.begin() + linearBegin, m_nodeList.begin() + linearEnd);

    node->children.erase(node->children.begin() + first, node->children.begin() + end);

    // Correct source rows in subsequent siblings, if any.
    int row = first;
    for (auto iter = node->children.begin() + first; iter != node->children.end(); ++iter, ++row)
        (*iter)->sourceRow = row;

    // Correct target rows in all linearly subsequent siblings.
    for (auto iter = m_nodeList.begin() + linearBegin; iter != m_nodeList.end(); ++iter)
        (*iter)->row -= linearCount;

    NX_ASSERT_HEAVY_CONDITION(debugCheckIntegrity());
}

std::pair<int, int> LinearizationListModel::Private::moveChildren(
    NodePtr sourceNode, int first, int count, NodePtr destinationNode, int destination)
{
    if (count == 0)
        return {};

    NX_CRITICAL(m_operationInProgress);

    NX_CRITICAL(sourceNode && first >= 0 && (first + count) <= sourceNode->childCount());
    NX_CRITICAL(destinationNode && destination >= 0
        && destination <= destinationNode->childCount());

    NX_VERBOSE(q, "Moving children from node at row=%1, first=%2, count=%3, to node at row=%4, "
        "destination=%5", sourceNode->row, first, count, destinationNode->row, destination);

    const int end = first + count;

    const int linearBegin = sourceNode->children[first]->row;
    const int linearEnd = nextRow(sourceNode->children[end - 1]);
    const int linearCount = linearEnd - linearBegin;

    const int linearDestination = destination < destinationNode->childCount()
        ? destinationNode->children[destination]->row
        : nextRow(destinationNode);

    NX_CRITICAL(linearDestination >= 0 && linearDestination <= rowCount()
        && (linearDestination <= linearBegin || linearDestination >= linearEnd));

    const bool moveWithoutReorder = linearDestination == linearBegin
        || linearDestination == linearEnd;

    NX_CRITICAL(moveWithoutReorder == m_moveWithoutReorder);

    const auto moveNodes =
        [](std::vector<NodePtr>& nodes, int sourceBegin, int sourceEnd, int destination,
            auto rowFieldPtr)
        {
            const auto [beginIndex, newBeginIndex, endIndex] = destination < sourceBegin
                ? std::make_tuple(destination, sourceBegin, sourceEnd)
                : std::make_tuple(sourceBegin, sourceEnd, destination);

            std::rotate(nodes.begin() + beginIndex, nodes.begin() + newBeginIndex,
                nodes.begin() + endIndex);

            for (int index = beginIndex; index < endIndex; ++index)
                (*nodes[index]).*rowFieldPtr = index;
        };

    if (sourceNode == destinationNode)
    {
        moveNodes(sourceNode->children, first, end, destination, &Node::sourceRow);
    }
    else
    {
        destinationNode->children.insert(destinationNode->children.begin() + destination,
            std::make_move_iterator(sourceNode->children.begin() + first),
            std::make_move_iterator(sourceNode->children.begin() + end));

        // Correct parent in the moved children sequence.
        for (auto iter = destinationNode->children.begin() + destination, endIter = iter + count;
            iter != endIter;
            ++iter)
        {
            (*iter)->parent = destinationNode;
        }

        // Correct sourceRow in the destination node.
        int row = destination;
        for (auto iter = destinationNode->children.begin() + destination;
            iter != destinationNode->children.end();
            ++iter, ++row)
            {
                (*iter)->sourceRow = row;
            }

        sourceNode->children.erase(
            sourceNode->children.begin() + first,
            sourceNode->children.begin() + end);

        // Correct sourceRow in the source node.
        row = first;
        for (auto iter = sourceNode->children.begin() + first;
            iter != sourceNode->children.end();
            ++iter, ++row)
        {
            (*iter)->sourceRow = row;
        }
    }

    if (!moveWithoutReorder)
        moveNodes(m_nodeList, linearBegin, linearEnd, linearDestination, &Node::row);

    NX_ASSERT_HEAVY_CONDITION(debugCheckIntegrity());

    return linearDestination <= linearBegin
        ? std::make_pair(linearDestination, linearCount)
        : std::make_pair(linearDestination - linearCount, linearCount);
}

void LinearizationListModel::Private::rebuildNode(NodePtr node)
{
    if (!NX_ASSERT(m_rebuiltNodes.empty()))
        m_rebuiltNodes.clear();

    const int firstRow = node->row + 1;
    const int oldNextRow = nextRow(node);

    int currentRow = firstRow;
    internalRebuildNode(node, currentRow);

    if (currentRow == oldNextRow)
    {
        // No insertions or removal: just replace nodes in the list.

        std::copy(
            std::make_move_iterator(m_rebuiltNodes.begin()),
            std::make_move_iterator(m_rebuiltNodes.end()),
            m_nodeList.begin() + firstRow);
    }
    else
    {
        // Insertions or removal has taken place.

        // Remove old nodes from the list.
        m_nodeList.erase(m_nodeList.begin() + firstRow, m_nodeList.begin() + oldNextRow);

        // Insert new nodes to the list.
        m_nodeList.insert(m_nodeList.begin() + firstRow,
            std::make_move_iterator(m_rebuiltNodes.begin()),
            std::make_move_iterator(m_rebuiltNodes.end()));

        // Update subsequent indices.
        for (auto iter = m_nodeList.begin() + currentRow;
            iter != m_nodeList.end();
            ++iter, ++currentRow)
        {
            (*iter)->row = currentRow;
        }
    }

    m_rebuiltNodes.clear();
}

void LinearizationListModel::Private::internalRebuildNode(NodePtr node, int& nextRow)
{
    const auto nodeIndex = sourceIndex(node);
    node->children.clear();

    if (node != m_rootNode && !isSourceExpanded(nodeIndex))
        return;

    const int newChildCount = m_sourceModel->rowCount(nodeIndex);
    if (newChildCount == 0)
        m_expandedSourceIndices.remove(nodeIndex);

    for (int i = 0; i < newChildCount; ++i)
    {
        auto child = std::make_shared<Node>(node, i, nextRow++);
        node->children.push_back(child);
        m_rebuiltNodes.push_back(child);
        internalRebuildNode(child, nextRow);
    }
}

LinearizationListModel::Private::NodePtr LinearizationListModel::Private::getNode(
    const QModelIndex& sourceIndex) const
{
    if (sourceIndex == m_sourceRoot)
        return m_rootNode;

    if (!sourceIndex.isValid())
        return {};

    const auto parentNode = getNode(sourceIndex.parent());
    if (!parentNode || parentNode->children.empty())
        return {};

    if (!NX_ASSERT(sourceIndex.row() < parentNode->childCount()))
        return {};

    return parentNode->children[sourceIndex.row()];
}

QModelIndex LinearizationListModel::Private::sourceIndex(const NodePtr& node) const
{
    if (node == m_rootNode || !NX_ASSERT(node && m_sourceModel))
        return m_sourceRoot;

    return m_sourceModel->index(node->sourceRow, 0, sourceIndex(node->parent.lock()));
}

QModelIndex LinearizationListModel::Private::index(const NodePtr& node) const
{
    return node ? q->index(node->row) : QModelIndex();
}

int LinearizationListModel::Private::nextRow(const NodePtr& after) const
{
    return after->children.empty()
        ? after->row + 1
        : nextRow(after->children.back());
}

bool LinearizationListModel::Private::debugCheckIntegrity() const
{
    int currentLinearIndex = 0;
    return debugCheckTreeIntegrity(m_rootNode, currentLinearIndex)
        && NX_ASSERT(currentLinearIndex == rowCount())
        && debugCheckLinearIntegrity();
}

bool LinearizationListModel::Private::debugCheckLinearIntegrity() const
{
    int i = 0;
    for (const auto& node: m_nodeList)
    {
        if (!NX_ASSERT(node->row == i, "[%1] Wrong row index %2 at position %3",
            nodePath(node), node->row, i))
        {
            return false;
        }
        ++i;
    }

    return true;
}

bool LinearizationListModel::Private::debugCheckTreeIntegrity(
    const NodePtr& from, int& currentLinearIndex) const
{
    int i = 0;
    for (const auto& child: from->children)
    {
        if (!NX_ASSERT(child->parent.lock() == from, "[%1] Wrong parent %2, expected %3",
                nodePath(child), nodePath(child->parent.lock()), nodePath(from))
            || !NX_ASSERT(child->sourceRow == i, "[%1] Wrong source row %2, expected %3",
                nodePath(child), child->sourceRow, i)
            || !NX_ASSERT(child->row == currentLinearIndex, "[%1] Wrong row %2, expected %3",
                nodePath(child), child->row, currentLinearIndex))
        {
            return false;
        }

        ++currentLinearIndex;
        ++i;

        if (!debugCheckTreeIntegrity(child, currentLinearIndex))
            return false;
    }

    return true;
}

} // namespace nx::vms::client::desktop
