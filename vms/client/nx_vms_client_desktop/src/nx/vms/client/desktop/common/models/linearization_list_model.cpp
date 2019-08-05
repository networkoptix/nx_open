#include "linearization_list_model.h"

#include <vector>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {

namespace {

QString toString(const QModelIndex& index)
{
    if (!index.isValid())
        return "!";

    return index.parent().isValid()
        ? QString("%1:%2").arg(toString(index.parent()), QString::number(index.row()))
        : QString::number(index.row());
}

} // namespace

class LinearizationListModel::Private: public QObject
{
    LinearizationListModel* const q;

public:
    Private(LinearizationListModel* q): q(q) {}

    QAbstractItemModel* sourceModel() const { return m_sourceModel.data(); }
    void setSourceModel(QAbstractItemModel* value);

    QModelIndex mapToSource(const QModelIndex& index) const;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const;

    int level(const QModelIndex& index) const;
    int rowCount() const;

    bool isSourceExpanded(const QModelIndex& sourceIndex) const;
    bool setExpanded(const QModelIndex& index, bool value);

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
        NodePtr parent;
        NodeList children;
        int sourceRow = 0;

        static constexpr int kRootNodeRow = -1;
        int row = kRootNodeRow;

        Node() = default;
        Node(const NodePtr& parent, int sourceRow, int row):
            parent(parent), sourceRow(sourceRow), row(row) {}
    };

    void reset();
    void insertChildren(NodePtr node, int position, int count, bool emitSignals = true);
    void removeChildren(NodePtr node, int first = 0, int count = -1, bool emitSignals = true);
    void rebuildNode(NodePtr node);
    void internalRebuildNode(NodePtr node, int& row);

    void expandRecursively(NodePtr node, bool force);

    NodePtr getNode(const QModelIndex& sourceIndex) const;

    QModelIndex sourceIndex(const NodePtr& node) const;
    QModelIndex index(const NodePtr& node) const;
    int nextRow(const NodePtr& after) const;

    bool debugCheckIntegrity() const;
    bool debugCheckLinearIntegrity() const;
    bool debugCheckTreeIntegrity(const NodePtr& from, int& currentLinearIndex) const;

    QString nodePath(const NodePtr& node) const { return toString(sourceIndex(node)); }

public:
    friend uint qHash(const NodePtr& node, uint seed) { return ::qHash(node.get(), seed); }

private:
    QPointer<QAbstractItemModel> m_sourceModel;
    QSet<QPersistentModelIndex> m_expandedSourceIndices;

    std::vector<PersistentIndexPair> m_changingIndices;
    QSet<NodePtr> m_layoutChangingNodes;

    // Rows insertion/removal operation.
    bool m_operationInProgress = false;

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
            return d->isSourceExpanded(sourceIndex);

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
            return d->setExpanded(index, value.toBool());

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

QHash<int, QByteArray> LinearizationListModel::roleNames() const
{
    auto result = base_type::roleNames();
    result.unite(QHash<int, QByteArray>({
        {LevelRole, "level"},
        {HasChildrenRole, "hasChildren"},
        {ExpandedRole, "expanded"}}));

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

    const auto notifier = nx::utils::makeScopeGuard(
        [this]() { emit q->sourceModelChanged({}); });

    ScopedReset scopedReset(q);

    if (m_sourceModel)
        m_sourceModel->disconnect(this);

    m_sourceModel = value;
    reset();

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

    // TODO: #vkutin Rows moving should probably be handled properly.

    connect(m_sourceModel, &QAbstractItemModel::rowsAboutToBeMoved,
        this, &Private::sourceModelAboutToBeReset);

    connect(m_sourceModel, &QAbstractItemModel::rowsMoved,
        this, &Private::sourceModelReset);

    connect(m_sourceModel, &QAbstractItemModel::layoutAboutToBeChanged,
        this, &Private::sourceLayoutAboutToBeChanged);

    connect(m_sourceModel, &QAbstractItemModel::layoutChanged,
        this, &Private::sourceLayoutChanged);
}

void LinearizationListModel::Private::sourceModelAboutToBeReset()
{
    q->beginResetModel();
}

void LinearizationListModel::Private::sourceModelReset()
{
    reset();
    q->endResetModel();
}

void LinearizationListModel::Private::sourceColumnsAboutToBeChanged(const QModelIndex& sourceParent)
{
    if (NX_ASSERT(m_sourceModel) && m_sourceModel->rowCount(sourceParent) > 0)
        sourceModelAboutToBeReset();
}

void LinearizationListModel::Private::sourceColumnsChanged(const QModelIndex& sourceParent)
{
    if (NX_ASSERT(m_sourceModel) && m_sourceModel->rowCount(sourceParent) > 0)
        sourceModelReset();
}

void LinearizationListModel::Private::sourceRowsAboutToBeInserted(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_CRITICAL(!m_operationInProgress);
    const auto node = getNode(sourceParent);

    if (!node || (node != m_rootNode && node->children.empty() /*might be not expanded*/))
        return;

    if (!NX_ASSERT(first >= 0 && first <= node->children.size() && last >= first))
        return;

    m_operationInProgress = true;

    const int linearPos = first < node->children.size()
        ? node->children[first]->row
        : nextRow(node);

    q->beginInsertRows({}, linearPos, linearPos + (last - first));
}

void LinearizationListModel::Private::sourceRowsInserted(
    const QModelIndex& sourceParent, int first, int last)
{
    const auto node = getNode(sourceParent);
    const int count = last - first + 1;

    if (m_operationInProgress && NX_ASSERT(node))
    {
        insertChildren(node, first, count, /*emitSignals*/false);
        m_operationInProgress = false;
        q->endInsertRows();
    }

    if (node && node != m_rootNode && m_sourceModel->rowCount(sourceParent) == count)
    {
        const auto parent = index(node);
        if (NX_ASSERT(parent.isValid()))
            emit q->dataChanged(parent, parent, {HasChildrenRole});
    }
}

void LinearizationListModel::Private::sourceRowsAboutToBeRemoved(
    const QModelIndex& sourceParent, int first, int last)
{
    NX_CRITICAL(!m_operationInProgress);

    const auto node = getNode(sourceParent);
    if (!node || (node != m_rootNode && node->children.empty()) /*might be not expanded*/)
        return;

    if (!NX_ASSERT(first >= 0 && last >= first && last < node->children.size()))
        return;

    const int linearBegin = node->children[first]->row;
    const int linearEnd = nextRow(node->children[last]);

    m_operationInProgress = true;
    q->beginRemoveRows({}, linearBegin, linearEnd - 1);
}

void LinearizationListModel::Private::sourceRowsRemoved(
    const QModelIndex& sourceParent, int first, int last)
{
    const auto node = getNode(sourceParent);
    if (m_operationInProgress && NX_ASSERT(node))
    {
        removeChildren(node, first, last - first + 1, /*emitSignals*/false);
        m_operationInProgress = false;
        q->endRemoveRows();
    }

    if (node && node != m_rootNode && !m_sourceModel->hasChildren(sourceParent))
    {
        const auto parent = index(node);
        if (NX_ASSERT(parent.isValid()))
        {
            emit q->dataChanged(parent, parent, m_expandedSourceIndices.remove(sourceParent)
                ? QVector<int>({HasChildrenRole, ExpandedRole})
                : QVector<int>({HasChildrenRole}));
        }
    }
}

void LinearizationListModel::Private::sourceDataChanged(const QModelIndex& sourceTopLeft,
    const QModelIndex& sourceBottomRight, const QVector<int>& roles)
{
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
    NX_CRITICAL(!m_operationInProgress && m_layoutChangingNodes.empty());
    if (hint != QAbstractItemModel::VerticalSortHint)
    {
        sourceModelAboutToBeReset();
    }
    else
    {
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
                if (node && !node->children.empty())
                    layoutChangingNodes.insert(node);
            }
        }

        if (layoutChangingNodes.empty())
            return;

        // Filter out nodes residing within changing parents.
        for (const auto& node: layoutChangingNodes)
        {
            bool ok = true;
            for (auto parent = node->parent; ok && parent; parent = parent->parent)
                ok = !layoutChangingNodes.contains(parent);

            if (ok)
                m_layoutChangingNodes.insert(node);
        }

        if (!NX_ASSERT(!m_layoutChangingNodes.empty()))
            return;

        emit q->layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);

        if (!NX_ASSERT(m_changingIndices.empty()))
            m_changingIndices.clear();

        const auto persistent = q->persistentIndexList();
        m_changingIndices.reserve(persistent.size());

        for (const auto& index: persistent)
            m_changingIndices.emplace_back(index, mapToSource(index));
    }
}

void LinearizationListModel::Private::sourceLayoutChanged(
    const QList<QPersistentModelIndex>& parents, QAbstractItemModel::LayoutChangeHint hint)
{
    NX_CRITICAL(!m_operationInProgress);
    if (hint != QAbstractItemModel::VerticalSortHint)
    {
        sourceModelReset();
    }
    else
    {
        if (m_layoutChangingNodes.empty())
            return;

        for (const auto& node: m_layoutChangingNodes)
            rebuildNode(node);

        m_layoutChangingNodes.clear();
        NX_ASSERT_HEAVY_CONDITION(debugCheckIntegrity());

        for (const auto& changed: m_changingIndices)
            q->changePersistentIndex(changed.first, mapFromSource(changed.second));

        m_changingIndices.clear();
        emit q->layoutChanged({}, QAbstractItemModel::VerticalSortHint);
    }
}

QModelIndex LinearizationListModel::Private::mapToSource(const QModelIndex& index) const
{
    if (!index.isValid() || !NX_ASSERT(q->checkIndex(index)))
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
    const auto sourceIndex = mapToSource(index);

    int level = -1;
    for (auto index = sourceIndex; index.isValid(); index = index.parent())
        ++level;

    return level;
}

int LinearizationListModel::Private::rowCount() const
{
    return int(m_nodeList.size());
}

bool LinearizationListModel::Private::isSourceExpanded(const QModelIndex& sourceIndex) const
{
    return m_expandedSourceIndices.contains(sourceIndex);
}

bool LinearizationListModel::Private::setExpanded(const QModelIndex& index, bool value)
{
    const auto sourceIndex = mapToSource(index);
    if (!sourceIndex.isValid()
        || isSourceExpanded(sourceIndex) == value
        || (value && !m_sourceModel->hasChildren(sourceIndex)))
    {
        return false;
    }

    const auto node = getNode(sourceIndex);
    if (!NX_ASSERT(node && node->children.empty() == value))
        return false;

    if (value)
    {
        // Expand.
        m_expandedSourceIndices.insert(sourceIndex);
        insertChildren(node, 0, m_sourceModel->rowCount(sourceIndex));
        expandRecursively(node, false);
    }
    else
    {
        // Collapse.
        removeChildren(node);
        m_expandedSourceIndices.remove(sourceIndex);
    }

    emit q->dataChanged(index, index, {ExpandedRole});
    return true;
}

void LinearizationListModel::Private::expandRecursively(NodePtr node, bool force)
{
    for (const auto& child: node->children)
    {
        const auto sourceIndex = this->sourceIndex(child);
        if (child->children.empty()
            && (force || m_expandedSourceIndices.contains(sourceIndex))
            && m_sourceModel->hasChildren(sourceIndex))
        {
            m_expandedSourceIndices.insert(sourceIndex);
            insertChildren(child, 0, m_sourceModel->rowCount(sourceIndex));
        }

        expandRecursively(child, force);
    }
}

void LinearizationListModel::Private::expandAll()
{
    if (m_sourceModel)
        expandRecursively(m_rootNode, true);
}

void LinearizationListModel::Private::collapseAll()
{
    for (const auto& child: m_rootNode->children)
        setExpanded(index(child), false);

    m_expandedSourceIndices.clear();
}

void LinearizationListModel::Private::reset()
{
    m_rootNode = std::make_shared<Node>();
    m_nodeList.clear();
    m_expandedSourceIndices.clear();

    if (!m_sourceModel)
        return;

    const int count = m_sourceModel->rowCount();
    m_rootNode->children.reserve(count);

    for (int row = 0; row < count; ++row)
    {
        auto child = std::make_shared<Node>(m_rootNode, row, row);
        m_rootNode->children.push_back(child);
        m_nodeList.push_back(child);
    }
}

void LinearizationListModel::Private::insertChildren(
    NodePtr node, int position, int count, bool emitSignals)
{
    if (!NX_CRITICAL(node && position >= 0 && position <= node->children.size()))
        return;

    const int linearPos = position < node->children.size()
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

    ScopedInsertRows scopedInsertRows(q, linearPos, linearPos + count - 1, emitSignals);

    for (auto iter = node->children.begin() + position; iter != node->children.end(); ++iter)
        (*iter)->sourceRow += count;

    node->children.insert(node->children.begin() + position, children.begin(), children.end());

    for (auto iter = m_nodeList.begin() + linearPos; iter != m_nodeList.end(); ++iter)
        (*iter)->row += count;

    m_nodeList.insert(m_nodeList.begin() + linearPos, std::make_move_iterator(children.begin()),
        std::make_move_iterator(children.end()));

    NX_ASSERT_HEAVY_CONDITION(debugCheckIntegrity());
}

void LinearizationListModel::Private::removeChildren(
    NodePtr node, int first, int count, bool emitSignals)
{
    if (count == 0 || !NX_CRITICAL(node && first >= 0))
        return;

    const int childrenCount = int(node->children.size());
    const int end = count >= 0 ? (first + count) : childrenCount;

    if (!NX_CRITICAL(end <= childrenCount))
        return;

    const int linearBegin = node->children[first]->row;
    const int linearEnd = nextRow(node->children[end - 1]);
    const int linearCount = linearEnd - linearBegin;

    ScopedRemoveRows scopedRemoveRows(q, linearBegin, linearEnd - 1, emitSignals);
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

void LinearizationListModel::Private::rebuildNode(NodePtr node)
{
    int baseRow = node->row;
    internalRebuildNode(node, baseRow);
}

void LinearizationListModel::Private::internalRebuildNode(NodePtr node, int& row)
{
    const auto nodeIndex = sourceIndex(node);
    node->children.clear();

    if (node != m_rootNode && !isSourceExpanded(nodeIndex))
        return;

    for (int i = 0, count = m_sourceModel->rowCount(nodeIndex); i < count; ++i)
    {
        auto child = std::make_shared<Node>(node, i, ++row);
        node->children.push_back(child);
        m_nodeList[child->row] = child;
        internalRebuildNode(child, row);
    }
}

LinearizationListModel::Private::NodePtr LinearizationListModel::Private::getNode(
    const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid())
        return m_rootNode;

    const auto parentNode = getNode(sourceIndex.parent());
    if (!parentNode || parentNode->children.empty())
        return {};

    if (!NX_ASSERT(sourceIndex.row() < parentNode->children.size()))
        return {};

    return parentNode->children[sourceIndex.row()];
}

QModelIndex LinearizationListModel::Private::sourceIndex(const NodePtr& node) const
{
    if (node == m_rootNode || !NX_ASSERT(node && m_sourceModel))
        return {};

    return m_sourceModel->index(node->sourceRow, 0, sourceIndex(node->parent));
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
        && NX_ASSERT(currentLinearIndex == m_nodeList.size())
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
        if (!NX_ASSERT(child->parent == from, "[%1] Wrong parent %2, expected %3",
                nodePath(child), nodePath(child->parent), nodePath(from))
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
