#include "selection_node_view_state_reducer.h"

#include "selection_view_node_helpers.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_helpers.h"
#include "../details/node/view_node_data_builder.h"
#include "../node_view/node_view_state_reducer.h"

namespace {

using namespace nx::vms::client::desktop::node_view;
using namespace nx::vms::client::desktop::node_view::details;

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
    const ColumnSet& columns,
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
            NX_ASSERT(false, "Shouldn't get here!");
            return 0;
    }
}

bool checkable(const ColumnSet& selectionColumns, const NodePtr& node)
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
 * Returns "non-all-check" nodes which are checkable.
 */
NodeList getSimpleCheckableNodes(const ColumnSet& selectionColumns, NodeList nodes)
{
    const auto newEnd = std::remove_if(nodes.begin(), nodes.end(),
        [selectionColumns](const NodePtr& node)
        {
            return !checkable(selectionColumns, node);
        });
    nodes.erase(newEnd, nodes.end());
    return nodes;
}

/**
 * Returns list of checkable siblings except specified node and "all-check" nodes.
 */
NodeList getSimpleCheckableSiblings(const ColumnSet& selectionColumns, const NodePtr& node)
{
    NodeList siblings = getSiblings(node);
    const auto nodePath = node->path();
    const auto itOtherCheckableEnd = std::remove_if(siblings.begin(), siblings.end(),
        [selectionColumns, nodePath](const  NodePtr& siblingNode)
        {
            return !checkable(selectionColumns, siblingNode);
        });

    siblings.erase(itOtherCheckableEnd, siblings.end());
    return siblings;
}

Qt::CheckState getSiblingsCheckState(
    const ColumnSet& selectionColumns,
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

// Forward declaration.
int setNodeCheckedInternal(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnSet& selectionColumns,
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
    const ColumnSet& selectionColumns,
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

    return selectionDiff;
}

/**
 * Tries to update parent (and all above, accordingly) state to calculated one.
 */
void updateStateUpside(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnSet& selectionColumns,
    const NodePtr& node,
    Qt::CheckState currentState,
    int upflowSelectionDiff)
{
    const auto siblings = getSimpleCheckableSiblings(selectionColumns, node);
    const auto siblingsState = getSiblingsCheckState(selectionColumns, currentState, siblings);

    const auto parent = node->parent();
    if (parent && !parent->isRoot())
    {
        setNodeCheckedInternal(patch, state, selectionColumns,
            parent->path(), siblingsState, UpsideFlag, upflowSelectionDiff);
    }
}

/**
 * @return
 */
int setNodeCheckedInternal(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ColumnSet& selectionColumns,
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

    // We process downside flow firstly to calculate selected children count information.
    int siblingsSelectionDiff = 0;

    if (flags.testFlag(DownsideFlag))
    {
        // Just updates all children items are updated.
        childrenSelectionDiff += updateStateDownside(patch, state, selectionColumns, node, checkedState);
    }

    int upflowSelectedDiff = siblingsSelectionDiff + childrenSelectionDiff + operationDiff;
    // Here we have selected children information and continue upside flow.
    if (flags.testFlag(UpsideFlag))
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

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

NodeViewStatePatch SelectionNodeViewStateReducer::setNodeSelected(
    const NodeViewState& state,
    const ColumnSet& selectionColumns,
    const ViewNodePath& path,
    Qt::CheckState nodeCheckedState)
{
    NodeViewStatePatch result;
    setNodeCheckedInternal(result, state, selectionColumns, path,
        nodeCheckedState, DownsideFlag | UpsideFlag, 0);

    return result;
}

} // namespace node_view
} // namespace nx::vms::client::desktop
