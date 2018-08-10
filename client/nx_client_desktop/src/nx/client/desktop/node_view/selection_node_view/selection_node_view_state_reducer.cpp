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

/**
 * @return 1 if node checked state become Qt::Checked or Qt::PartiallyChecked,
 * -1 if node checked state has changed to Qt::Unchecked, otherwise 0.
 */
int addCheckStateChangeToPatch(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ViewNodePath& path,
    const ColumnsSet& columns,
    Qt::CheckState nodeCheckedState)
{
    const auto node = state.nodeByPath(path);
    const auto checkStatePatch = NodeViewStateReducer::setNodeChecked(node, columns, nodeCheckedState);

    if (checkStatePatch.steps.empty())
        return 0; //< Nothing changed

    patch.appendPatchSteps(checkStatePatch);

    const int anyColumn = *columns.begin();
    switch(checkedState(node, anyColumn))
    {
        case Qt::Checked:
            return nodeCheckedState == Qt::Unchecked ? -1 : 0;
        case Qt::PartiallyChecked:
            return nodeCheckedState == Qt::Unchecked ? -1 : 0;
        case Qt::Unchecked:
            return 1;
        default:
            NX_EXPECT(false, "Shouldn't get here!");
            return 0;
    }
}

bool checkable(const ColumnsSet& selectionColumns, const NodePtr& node)
{
    return std::any_of(selectionColumns.begin(), selectionColumns.end(),
        [node](const int column) { return checkable(node, column); });
}

NodeList getSiblings(const NodePtr& node)
{
    const auto parent = node->parent();
    if (!parent)
        return NodeList(); //< No siblings for root node.

    auto result = parent->children();
    const auto nodePath = node->path();
    const auto itEnd = std::remove_if(result.begin(), result.end(),
        [nodePath](const NodePtr& other) { return other->path() == nodePath; });
    result.erase(itEnd, result.end());
    return result;
}

/**
 * Returns only "all-check" nodes from specified ones.
 */
NodeList getAllCheckNodes(NodeList nodes)
{
    const auto itOtherCheckableEnd = std::remove_if(nodes.begin(), nodes.end(),
        [](const  NodePtr& siblingNode) { return !checkAllNode(siblingNode); });

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
            return !checkable(selectionColumns, node) || checkAllNode(node);
        });
    nodes.erase(newEnd, nodes.end());
    return nodes;
}

/**
 * Returns list of checkable siblings except specified node and "all-check" nodes.
 */
NodeList getSimpleCheckableSiblings(const ColumnsSet& selectionColumns, const NodePtr& node)
{
    NodeList siblings = getSiblings(node);
    const auto nodePath = node->path();
    const auto itOtherCheckableEnd = std::remove_if(siblings.begin(), siblings.end(),
        [selectionColumns, nodePath](const  NodePtr& siblingNode)
        {
            return !checkable(selectionColumns, siblingNode) || checkAllNode(siblingNode);
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
int setAllCheckNodesState(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& columns,
    const NodeList& nodes,
    Qt::CheckState checkedState)
{
    int checkAllSelectionDiff = 0;
    for (const auto checkAllSibling: getAllCheckNodes(nodes))
    {
        checkAllSelectionDiff += addCheckStateChangeToPatch(
            patch, state, checkAllSibling->path(), columns, checkedState);
    }
    return checkAllSelectionDiff;
}

// Forward declaration.
int setNodeCheckedInternal(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& selectionColumns,
    const ViewNodePath& path,
    Qt::CheckState checkedState,
    CheckChangesFlags flags,
    int upflowSelectionDiff);

/**
 * Tries to set all children nodes to the specified state.
 * @return Selection difference number.
 */
int updateStateDownside(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& selectionColumns,
    const NodePtr& node,
    Qt::CheckState checkedState)
{
    int selectionDiff = 0;
    const auto children = node->children();
    for (const auto& child: getSimpleCheckableNodes(selectionColumns, children))
    {
        selectionDiff += setNodeCheckedInternal(patch, state, selectionColumns,
            child->path(), checkedState, DownsideFlag, 0);
    }

    selectionDiff += setAllCheckNodesState(patch, state, selectionColumns, children, checkedState);
    return selectionDiff;
}

/**
 * Tries to update parent (and all above, accordingly) state to calculated one.
 */
void updateStateUpside(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& selectionColumns,
    const NodePtr& node,
    Qt::CheckState currentState,
    int upflowSelectionDiff)
{
    const auto siblings = getSimpleCheckableSiblings(selectionColumns, node);
    const auto siblingsState = getSiblingsCheckState(selectionColumns, currentState, siblings);

    const int allCheckSelectionDiff =
        setAllCheckNodesState(patch, state, selectionColumns, getSiblings(node), siblingsState);

    const auto parent = node->parent();
    if (parent && !parent->isRoot())
    {
        setNodeCheckedInternal(patch, state, selectionColumns,
            parent->path(), siblingsState, UpsideFlag, upflowSelectionDiff + allCheckSelectionDiff);
    }
}

/**
 * @return
 */
int setNodeCheckedInternal(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnsSet& selectionColumns,
    const ViewNodePath& path,
    Qt::CheckState checkedState,
    CheckChangesFlags flags,
    int initialUpflowSelectionDiff)
{
    const auto node = state.nodeByPath(path);
    if (!node)
        return 0;

    const int operationDiff =
        addCheckStateChangeToPatch(patch, state, path, selectionColumns, checkedState);
    int childrenSelectionDiff = initialUpflowSelectionDiff;

    const bool initialChange = flags.testFlag(UpsideFlag) && flags.testFlag(DownsideFlag);
    const auto allSiblingsCheckNode = checkAllNode(node);
    NX_EXPECT(!allSiblingsCheckNode || initialChange, "Shouldn't get here!");

    const auto siblings = allSiblingsCheckNode
        ? getSimpleCheckableSiblings(selectionColumns, node)
        : NodeList();

    // We process downside flow firstly to calculate selected children count information.
    int siblingsSelectionDiff = 0;
    if (allSiblingsCheckNode)
    {
        NX_EXPECT(checkedState != Qt::PartiallyChecked, "Partial state should be handled manually!");
        for (const auto sibling: siblings)
        {
            siblingsSelectionDiff += setNodeCheckedInternal(patch, state, selectionColumns,
                sibling->path(), checkedState, DownsideFlag, 0);
        }
    }
    else if (flags.testFlag(DownsideFlag))
    {
        // Just updates all children items are updated.
        childrenSelectionDiff += updateStateDownside(patch, state, selectionColumns, node, checkedState);
    }

    int upflowSelectedDiff = siblingsSelectionDiff + childrenSelectionDiff + operationDiff;
    // Here we have selected children information and continue upside flow.
    if (allSiblingsCheckNode)
    {
        upflowSelectedDiff += setAllCheckNodesState(
            patch, state, selectionColumns, getSiblings(node), checkedState);

        if (const auto parent = node->parent())
        {
            setNodeCheckedInternal(patch, state, selectionColumns,
                parent->path(), checkedState, UpsideFlag, upflowSelectedDiff);
        }
    }
    else if (flags.testFlag(UpsideFlag))
    {
        updateStateUpside(patch, state, selectionColumns, node, checkedState, upflowSelectedDiff);
    }

    if (childrenSelectionDiff)
    {
        ViewNodeData selectionCountData;
        const int nodeSelectedChildrenCount = selectedChildrenCount(node) + childrenSelectionDiff;
        setSelectedChildrenCount(selectionCountData, nodeSelectedChildrenCount);
        patch.addChangeStep(node->path(), selectionCountData);
    }
    return upflowSelectedDiff;
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
    Qt::CheckState nodeCheckedState)
{
    NodeViewStatePatch result;
    setNodeCheckedInternal(result, state, selectionColumns, path,
        nodeCheckedState, DownsideFlag | UpsideFlag, 0);

    return result;
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

