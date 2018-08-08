#include "selection_node_view_state_reducer.h"

#include "selection_view_node_helpers.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_helpers.h"
#include "../details/node/view_node_data_builder.h"
#include "../node_view/node_view_state_reducer.h"

namespace {

using namespace nx::client::desktop::node_view;
using namespace nx::client::desktop::node_view::details;

enum CheckChangesFlag
{
    DownsideFlag    = 0x1,
    UpsideFlag      = 0x2,
};

Q_DECLARE_FLAGS(CheckChangesFlags, CheckChangesFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(CheckChangesFlags)

void addCheckStateChangeToPatch(
    NodeViewStatePatch& patch,
    const ViewNodePath& path,
    const ColumnsSet& columns,
    Qt::CheckState state)
{
    patch.appendPatchSteps(NodeViewStateReducer::setNodeChecked(path, columns, state));
}

bool checkable(const ColumnsSet& selectionColumns, const NodePtr& node)
{
    return std::any_of(selectionColumns.begin(), selectionColumns.end(),
        [node](const int column) { return checkable(node, column); });
}

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

NodeList getAllCheckSiblings(const NodePtr& node)
{
    const auto parent = node->parent();
    return getAllCheckNodes(parent ? parent->children() : NodeList());
}

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

// Returns list of checkable siblings except specified node and All-Sibling-Check nodes.
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

// TODO: get rid of duplicate code
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

    addCheckStateChangeToPatch(patch, path, selectionColumns, checkedState);

    const bool initialChange = flags.testFlag(UpsideFlag) && flags.testFlag(DownsideFlag);
    const auto siblings = getSimpleCheckableSiblings(selectionColumns, node);
    const auto allSiblingsCheckNode = isCheckAllNode(node);
    NX_EXPECT(!allSiblingsCheckNode || initialChange, "Shouldn't get here!");
    if (allSiblingsCheckNode)
    {
        NX_EXPECT(checkedState != Qt::PartiallyChecked);
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
    }
    else
    {
        // Usual node. Just check if all children/parent items are updated.
        // Fill up states for All-Sibling-Check nodes

        const auto siblingsState = getSiblingsCheckState(selectionColumns, checkedState, siblings);
        const auto checkAllState = initialChange || flags.testFlag(UpsideFlag)
            ? siblingsState : checkedState;

        const auto checkAllSiblings = getAllCheckSiblings(node);
        for (const auto checkAllSibling: checkAllSiblings)
        {
            addCheckStateChangeToPatch(patch, checkAllSibling->path(),
                selectionColumns, checkAllState);
        }

        if (flags.testFlag(DownsideFlag))
        {
            // Just tries to set all children nodes to the same state.
            const auto children = node->children();
            for (const auto& child: getSimpleCheckableNodes(selectionColumns, children))
            {
                setNodeCheckedInternal(patch, state, selectionColumns,
                    child->path(), checkedState, DownsideFlag);
            }

            for (const auto& childCheckAll: getAllCheckNodes(children))
            {
                addCheckStateChangeToPatch(patch, childCheckAll->path(),
                    selectionColumns, checkedState);
            }
        }

        if (flags.testFlag(UpsideFlag))
        {
            // Just tries to update parent (and all above, accordingly) state to calculated one.
            const auto parent = node->parent();
            if (parent && !parent->isRoot() && checkable(selectionColumns, parent))
            {
                setNodeCheckedInternal(patch, state, selectionColumns,
                    parent->path(), siblingsState, UpsideFlag);
            }
        }
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
    int column,
    Qt::CheckState checkedState)
{
    if (!selectionColumns.contains(column))
        return NodeViewStatePatch();

    NodeViewStatePatch result;
    setNodeCheckedInternal(result, state, selectionColumns, path,
        checkedState, DownsideFlag | UpsideFlag);
    return result;
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

