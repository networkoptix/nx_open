#include "node_view_state_reducer.h"

#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>

namespace {

using namespace nx::client::desktop;

enum class Direction
{
    Downside,
    Upside,
    Both
};

void setNodeCheckedInternal(
    NodeViewStatePatch& patch,
    const NodeViewState& state,
    const ViewNodePath& path,
    Qt::CheckState checkedState,
    Direction direction = Direction::Both)
{
    const auto node = state.nodeByPath(path);
    if (!node)
    {
        NX_EXPECT(false, "Wrong node path!");
        return;
    }

    const auto nodeFlagsValue = node->data(node_view::nameColumn, node_view::nodeFlagsRole);
    const auto nodeFlags = nodeFlagsValue.value<node_view::NodeFlags>();
    const bool checkAllSiblingsNode = nodeFlags.testFlag(node_view::AllSiblingsCheckFlag);
//    if (checkAllSiblingsNode)
//    {

//    }
//    else
    {
        // Usual node. Just check if all children/parent items are updated.
        if (!node->checkable())
            return;

        auto nodeDescirption = NodeViewStatePatch::NodeDescription({path});
        nodeDescirption.data.data[node_view::checkMarkColumn][Qt::CheckStateRole] = checkedState;
        patch.changedData.append(nodeDescirption);

        if (direction != Direction::Upside)
        {
            for (const auto& child: node->children())
            {
                setNodeCheckedInternal(patch, state, child->path(),
                    checkedState, Direction::Downside);
            }
        }

        if (direction != Direction::Downside)
        {
            const auto parent = node->parent();
            NodeList siblings = parent->children();
            const auto itOtherCheckableEnd =
                std::remove_if(siblings.begin(), siblings.end(),
                    [nodePath = node->path()](const  NodePtr& siblingNode)
                    {
                        return !siblingNode->checkable() || siblingNode->path() == nodePath;
                    });

            siblings.erase(itOtherCheckableEnd, siblings.end());
            const auto getParentCheckedState =
                [siblings, checkedState]()
                {

                    for (auto it = siblings.begin(); it != siblings.end(); ++it)
                    {
                        const auto state = helpers::checkedState(*it);
                        if (state != checkedState)
                            return Qt::PartiallyChecked;
                    }
                    return checkedState;
                };

            const auto parentState = siblings.isEmpty()
                ? checkedState
                : getParentCheckedState();
            setNodeCheckedInternal(patch, state, parent->path(), parentState, Direction::Upside);
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
    setNodeCheckedInternal(patch, state, path, checkedState);
    return patch;
}

} // namespace desktop
} // namespace client
} // namespace nx

