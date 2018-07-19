#include "node_view_state_reducer.h"

#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_data_builder.h>

namespace {

using namespace nx::client::desktop;

enum CheckChangesFlag
{
    DownsideFlag    = 0x1,
    UpsideFlag      = 0x2,
};

Q_DECLARE_FLAGS(CheckChangesFlags, CheckChangesFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(CheckChangesFlags)

bool isAllSiblingsCheckNode(const NodePtr& node)
{
    const auto nodeFlagsValue = node->data(node_view::nameColumn, node_view::nodeFlagsRole);
    const auto nodeFlags = nodeFlagsValue.value<node_view::NodeFlags>();
    return nodeFlags.testFlag(node_view::AllSiblingsCheckFlag);
}

void addCheckStateChange(NodeViewStatePatch& patch, const ViewNodePath& path, Qt::CheckState state)
{
    auto nodeDescirption = NodeViewStatePatch::NodeDescription({path});
    ViewNodeDataBuilder(nodeDescirption.data).withCheckedState(state);
    patch.changedData.push_back(nodeDescirption);
}

NodeList getAllCheckNodes(NodeList nodes)
{
    if (nodes.isEmpty())
        return nodes;

    const auto itOtherCheckableEnd = std::remove_if(nodes.begin(), nodes.end(),
        [](const  NodePtr& siblingNode)
        {
            return !isAllSiblingsCheckNode(siblingNode);
        });

    nodes.erase(itOtherCheckableEnd, nodes.end());
    return nodes;
}

NodeList getAllCheckSiblings(const NodePtr& node)
{
    const auto parent = node->parent();
    return getAllCheckNodes(parent ? parent->children() : NodeList());
}

NodeList getSimpleCheckableNodes(NodeList nodes)
{
    const auto newEnd = std::remove_if(nodes.begin(), nodes.end(),
        [](const NodePtr& node)
        {
            return !helpers::checkableNode(node) || isAllSiblingsCheckNode(node);
        });
    nodes.erase(newEnd, nodes.end());
    return nodes;
}

// Returns list of checkable siblings except specified node and All-Sibling-Check node.
NodeList getSimpleCheckableSiblings(const NodePtr& node)
{
    const auto parent = node->parent();
    if (!parent)
        return NodeList();

    NodeList siblings = parent->children();
    const auto nodePath = node->path();
    const auto itOtherCheckableEnd = std::remove_if(siblings.begin(), siblings.end(),
        [nodePath](const  NodePtr& siblingNode)
        {
            return !helpers::checkableNode(siblingNode)
                || siblingNode->path() == nodePath
                || isAllSiblingsCheckNode(siblingNode);
        });

    siblings.erase(itOtherCheckableEnd, siblings.end());
    return siblings;
}

Qt::CheckState getSiblingsCheckState(Qt::CheckState currentCheckedState, const NodeList& siblings)
{
    if (siblings.isEmpty())
        return currentCheckedState;

    for (auto it = siblings.begin(); it != siblings.end(); ++it)
    {
        const auto state = helpers::nodeCheckedState(*it);
        if (state != currentCheckedState)
            return Qt::PartiallyChecked;
    }
    return currentCheckedState;
}

// TODO: get rid of duplicate code
void setNodeCheckedInternal(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ViewNodePath& path,
    Qt::CheckState checkedState,
    CheckChangesFlags flags)
{
    const auto node = state.nodeByPath(path);
    if (!node)
    {
        NX_EXPECT(false, "Wrong node path!");
        return;
    }

    if (!helpers::checkableNode(node))
        return;

    addCheckStateChange(patch, path, checkedState);

    const bool initialChange = flags.testFlag(UpsideFlag) && flags.testFlag(DownsideFlag);
    const auto siblings = getSimpleCheckableSiblings(node);
    const auto allSiblingsCheckNode = isAllSiblingsCheckNode(node);
    NX_EXPECT(!allSiblingsCheckNode || initialChange, "Shouldn't get here!");
    if (allSiblingsCheckNode)
    {
        NX_EXPECT(checkedState != Qt::PartiallyChecked);
        for (const auto sibling: siblings)
            setNodeCheckedInternal(patch, state, sibling->path(), checkedState, DownsideFlag);
        const auto parent = node->parent();
        if (parent && helpers::checkableNode(parent))
            setNodeCheckedInternal(patch, state, parent->path(), checkedState, UpsideFlag);
    }
    else
    {
        // Usual node. Just check if all children/parent items are updatedr.
        // Fill up states for All-Sibling-Check nodes

        const auto siblingsState = getSiblingsCheckState(checkedState, siblings);
        const auto checkAllState = initialChange || flags.testFlag(UpsideFlag)
            ? siblingsState : checkedState;

        const auto checkAllSiblings = getAllCheckSiblings(node);
        for (const auto checkAllSibling: checkAllSiblings)
            addCheckStateChange(patch, checkAllSibling->path(), checkAllState);

        if (flags.testFlag(DownsideFlag))
        {
            // Just tries to set all children nodes to the same state.
            const auto children = node->children();
            for (const auto& child: getSimpleCheckableNodes(children))
                setNodeCheckedInternal(patch, state, child->path(), checkedState, DownsideFlag);

            for (const auto& childCheckAll: getAllCheckNodes(children))
                addCheckStateChange(patch, childCheckAll->path(), checkedState);
        }

        if (flags.testFlag(UpsideFlag))
        {
            // Just tries to update parent (and all above, accordingly) state to calculated one.
            const auto parent = node->parent();
            if (parent && helpers::checkableNode(parent))
                setNodeCheckedInternal(patch, state, parent->path(), siblingsState, UpsideFlag);
        }
    }
}

} // namespace

namespace nx {
namespace client {
namespace desktop {


NodeViewStatePatch NodeViewStateReducer::setNodeChecked(
    const NodeViewState& state,
    const ViewNodePath& path,
    Qt::CheckState checkedState)
{
    NodeViewStatePatch patch;
    setNodeCheckedInternal(patch, state, path, checkedState, DownsideFlag | UpsideFlag);
    return patch;
}

} // namespace desktop
} // namespace client
} // namespace nx

