#include "selection_node_view_state_reducer.h"

#include "selection_view_node_helpers.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_helpers.h"
#include "../details/node/view_node_data_builder.h"
#include "../node_view/node_view_state_reducer.h"

namespace {

using namespace nx::client::desktop::node_view;
using namespace nx::client::desktop::node_view::details;

/**
 * Represents direction of checking flow.
 */
enum CheckChangesFlag
{
    DownsideFlag    = 0x1,
    UpsideFlag      = 0x2,
    AllFlags        = UpsideFlag | DownsideFlag
};

Q_DECLARE_FLAGS(CheckChangesFlags, CheckChangesFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(CheckChangesFlags)

void addCheckStateChangeToPatch(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ViewNodePath& path,
    const ColumnsSet& columns,
    Qt::CheckState checkedState)
{
    patch.appendPatchSteps(NodeViewStateReducer::setNodeChecked(state, path, columns, checkedState));
}

bool checkable(const ColumnsSet& selectionColumns, const NodePtr& node)
{
    return std::any_of(selectionColumns.begin(), selectionColumns.end(),
        [node](const int column) { return checkable(node, column); });
}

/**
 * Returns only "all-check" nodes from specified ones.
 */
NodeList getAllCheckNodes(NodeList nodes)
{
    if (nodes.isEmpty())
        return nodes;

    const auto itOtherCheckableEnd = std::remove_if(nodes.begin(), nodes.end(),
        [](const  NodePtr& siblingNode)
        {
            return !isCheckAllNode(siblingNode);
        });

    nodes.erase(itOtherCheckableEnd, nodes.end());
    return nodes;
}

/**
 * Returns "non-all-check" nodes which are checkable.
 */
NodeList getSimpleCheckableNodes(const ColumnsSet& selectionColumns, NodeList nodes)
{
    const auto newEnd = std::remove_if(nodes.begin(), nodes.end(),
        [selectionColumns](const NodePtr& node)
        {
            return !checkable(selectionColumns, node) || isCheckAllNode(node);
        });
    nodes.erase(newEnd, nodes.end());
    return nodes;
}

/**
 * Returns list of checkable siblings except specified node and "all-check" nodes.
 */
NodeList getSimpleCheckableSiblings(const ColumnsSet& selectionColumns, const NodePtr& node)
{
    const auto parent = node->parent();
    if (!parent)
        return NodeList();

    NodeList siblings = parent->children();
    const auto nodePath = node->path();
    const auto itOtherCheckableEnd = std::remove_if(siblings.begin(), siblings.end(),
        [selectionColumns, nodePath](const  NodePtr& siblingNode)
        {
            return !checkable(selectionColumns, siblingNode)
                || siblingNode->path() == nodePath
                || isCheckAllNode(siblingNode);
        });

    siblings.erase(itOtherCheckableEnd, siblings.end());
    return siblings;
}

Qt::CheckState getSiblingsCheckState(
    const ColumnsSet& selectionColumns,
    Qt::CheckState currentCheckedState,
    const NodeList& siblings)
{
    if (siblings.isEmpty() || selectionColumns.isEmpty())
        return currentCheckedState;

    const int anyColumn = *selectionColumns.begin();
    for (auto it = siblings.begin(); it != siblings.end(); ++it)
    {
        const auto state = checkedState(*it, anyColumn);
        if (state != currentCheckedState)
            return Qt::PartiallyChecked;
    }
    return currentCheckedState;
}

/**
 * Sets checked state of specified "all-check" nodes.
 */
void setAllCheckNodesState(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& columns,
    const NodeList& nodes,
    Qt::CheckState checkedState)
{
    for (const auto checkAllSibling: getAllCheckNodes(nodes))
    {
        addCheckStateChangeToPatch(patch, state, checkAllSibling->path(), columns, checkedState);
    }
}

/**
 * Sets checked state of "all-check" siblings for the node.
 */
void setSiblingsAllCheckNodesState(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& columns,
    const NodePtr& node,
    Qt::CheckState checkedState)
{
    if (const auto parent = node->parent())
        setAllCheckNodesState(patch, state, columns, parent->children(), checkedState);
}

// Forward declaration.
void setNodeCheckedInternal(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& selectionColumns,
    const ViewNodePath& path,
    Qt::CheckState checkedState,
    CheckChangesFlags flags);

/**
 * Tries to set all children nodes to the specified state.
 */
void updateStateDownside(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& selectionColumns,
    const NodePtr& node,
    Qt::CheckState checkedState)
{
    if (!node)
    {
        NX_EXPECT(false, "Node is null!");
        return;
    }

    const auto children = node->children();
    for (const auto& child: getSimpleCheckableNodes(selectionColumns, children))
    {
        setNodeCheckedInternal(patch, state, selectionColumns,
            child->path(), checkedState, DownsideFlag);
    }

    setAllCheckNodesState(patch, state, selectionColumns, children, checkedState);
}

/**
 * Tries to update parent (and all above, accordingly) state to calculated one.
 */
void updateStateUpside(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& selectionColumns,
    const NodePtr& node,
    Qt::CheckState currentState)
{
    const auto siblings = getSimpleCheckableSiblings(selectionColumns, node);
    const auto siblingsState = getSiblingsCheckState(selectionColumns, currentState, siblings);
    setSiblingsAllCheckNodesState(patch, state, selectionColumns, node, siblingsState);

    const auto parent = node->parent();
    if (parent && !parent->isRoot() && checkable(selectionColumns, parent))
    {
        setNodeCheckedInternal(patch, state, selectionColumns,
            parent->path(), siblingsState, UpsideFlag);
    }
}

void setNodeCheckedInternal(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& selectionColumns,
    const ViewNodePath& path,
    Qt::CheckState checkedState,
    CheckChangesFlags flags)
{
    const auto node = state.nodeByPath(path);
    if (!node || !checkable(selectionColumns, node))
        return;

    addCheckStateChangeToPatch(patch, state, path, selectionColumns, checkedState);

    const bool initialChange = flags.testFlag(UpsideFlag) && flags.testFlag(DownsideFlag);
    const auto allSiblingsCheckNode = isCheckAllNode(node);
    NX_EXPECT(!allSiblingsCheckNode || initialChange, "Shouldn't get here!");

    if (allSiblingsCheckNode)
    {
        const auto siblings = getSimpleCheckableSiblings(selectionColumns, node);
        NX_EXPECT(checkedState != Qt::PartiallyChecked, "Partial state should be handled manually!");
        for (const auto sibling: siblings)
        {
            setNodeCheckedInternal(patch, state, selectionColumns,
                sibling->path(), checkedState, DownsideFlag);
        }

        const auto parent = node->parent();
        if (parent && checkable(selectionColumns, parent))
        {
            setNodeCheckedInternal(patch, state, selectionColumns,
                parent->path(), checkedState, UpsideFlag);
        }

        // Updates state of sibling "all-check" nodes.
        setSiblingsAllCheckNodesState(patch, state, selectionColumns, node, checkedState);
    }
    else
    {
        // Usual node. Just check if all children/parent items are updated.
        if (flags.testFlag(DownsideFlag))
            updateStateDownside(patch, state, selectionColumns, node, checkedState);

        if (flags.testFlag(UpsideFlag))
            updateStateUpside(patch, state, selectionColumns, node, checkedState);
    }
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

using namespace details;

NodeViewStatePatch SelectionNodeViewStateReducer::setNodeSelected(
    const NodeViewState& state,
    const ColumnsSet& selectionColumns,
    const ViewNodePath& path,
    Qt::CheckState checkedState)
{
    NodeViewStatePatch result;
    setNodeCheckedInternal(result, state, selectionColumns, path,
        checkedState, DownsideFlag | UpsideFlag);
    return result;
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

