#include "resource_node_view_state_reducer.h"

#include <core/resource/resource.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_state_reducer_helpers.h>

namespace {

using namespace nx::client::desktop;

using ResourceIdSet = QSet<QnUuid>;

// Split to the generoc function and move part to the helprs namespace

OptionalCheckedState setResourcesCheckedInternal(
    NodeViewStatePatch& patch,
    const NodePtr& node,
    const ResourceIdSet& ids,
    Qt::CheckState targetState)
{
    const bool checkableNode = helpers::checkableNode(node);
    if (node->isLeaf())
    {
        if (!checkableNode)
            return OptionalCheckedState();

        const auto nodeResource = helpers::getResource(node);
        const auto id = nodeResource ? nodeResource->getId() : QnUuid();
        const auto checkedState = helpers::nodeCheckedState(node);
        if (id.isNull() || !ids.contains(id) || checkedState == targetState)
            return checkedState;

        helpers::addCheckStateChangeToPatch(patch, node->path(), targetState);
        return targetState;
    }

    NodeList allSiblingsCheckNodes;
    OptionalCheckedState cumulativeState;
    for (const auto child: node->children())
    {
        if (helpers::isAllSiblingsCheckNode(child))
        {
            allSiblingsCheckNodes.append(child);
            continue;
        }

        const auto childCheckState = setResourcesCheckedInternal(patch, child, ids, targetState);
        if (!childCheckState)
            continue;

        if (!cumulativeState)
            cumulativeState = *childCheckState;
        else if (*cumulativeState != *childCheckState)
            cumulativeState = Qt::PartiallyChecked;
    }

    if (!cumulativeState && checkableNode)
        return helpers::nodeCheckedState(node);

    for (const auto allSiblingsCheckNode: allSiblingsCheckNodes)
        helpers::addCheckStateChangeToPatch(patch, allSiblingsCheckNode->path(), *cumulativeState);

    if (!checkableNode)
        return OptionalCheckedState();

    helpers::addCheckStateChangeToPatch(patch, node->path(), *cumulativeState);
    return cumulativeState;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

NodeViewStatePatch ResourceNodeViewStateReducer::getLeafResourcesCheckedPatch(
    const NodeViewState& state,
    const QnResourceList& resources)
{
    NodeViewStatePatch patch;
    ResourceIdSet ids;
    for (const auto resource: resources)
        ids.insert(resource->getId());

    setResourcesCheckedInternal(patch, state.rootNode, ids, Qt::Checked);
    return patch;
}

} // namespace desktop
} // namespace client
} // namespace nx
