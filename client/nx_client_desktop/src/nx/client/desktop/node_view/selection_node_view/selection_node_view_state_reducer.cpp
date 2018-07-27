#include "selection_node_view_state_reducer.h"

#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_state_reducer_helpers.h>

namespace {

using namespace nx::client::desktop;
using namespace nx::client::desktop::details;

enum CheckChangesFlag
{
    DownsideFlag    = 0x1,
    UpsideFlag      = 0x2,
};

Q_DECLARE_FLAGS(CheckChangesFlags, CheckChangesFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(CheckChangesFlags)

bool isCheckable(const ColumnsSet& selectionColumns, const NodePtr& node)
{
    return std::any_of(selectionColumns.begin(), selectionColumns.end(),
        [node](const int column)
        {
            return node_view::helpers::isCheckable(node, column);
        });
}

NodeList getAllCheckNodes(NodeList nodes)
{
    if (nodes.isEmpty())
        return nodes;

    const auto itOtherCheckableEnd = std::remove_if(nodes.begin(), nodes.end(),
        [](const  NodePtr& siblingNode)
        {
            return !node_view::helpers::isAllSiblingsCheckNode(siblingNode);
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
            return !isCheckable(selectionColumns, node)
                || node_view::helpers::isAllSiblingsCheckNode(node);
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
            return !isCheckable(selectionColumns, siblingNode)
                || siblingNode->path() == nodePath
                || node_view::helpers::isAllSiblingsCheckNode(siblingNode);
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
        const auto state = node_view::helpers::checkedState(*it, anyColumn);
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
    if (!node || !isCheckable(selectionColumns, node))
        return;

    addCheckStateChangeToPatch(patch, path, selectionColumns, checkedState);

    const bool initialChange = flags.testFlag(UpsideFlag) && flags.testFlag(DownsideFlag);
    const auto siblings = getSimpleCheckableSiblings(selectionColumns, node);
    const auto allSiblingsCheckNode = node_view::helpers::isAllSiblingsCheckNode(node);
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
        if (parent && isCheckable(selectionColumns, parent))
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
            if (parent && !parent->isRoot() && isCheckable(selectionColumns, parent))
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

