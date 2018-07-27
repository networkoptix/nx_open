#include "resource_node_view_state_reducer.h"

#include <core/resource/resource.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_state_reducer_helpers.h>
#include <nx/client/desktop/resource_views/node_view/resource_node_view/resource_view_node_helpers.h>
namespace {

using namespace nx::client::desktop;
using namespace nx::client::desktop::details;

using ResourceIdSet = QSet<QnUuid>;

// Split to the generoc function and move part to the helprs namespace

bool isCheckable(const ColumnsSet& selectionColumns, const NodePtr& node)
{
    return std::any_of(selectionColumns.begin(), selectionColumns.end(),
        [node](const int column)
        {
            return node_view::helpers::isCheckable(node, column);
        });
}

// TODO: move to Selection Node View
OptionalCheckedState setResourcesCheckedInternal(
    NodeViewStatePatch& patch,
    const ColumnsSet& columns,
    const NodePtr& node,
    const ResourceIdSet& ids,
    Qt::CheckState targetState)
{
    const bool checkableNode = isCheckable(columns, node) && !node->isRoot();
    if (node->isLeaf())
    {
        if (!checkableNode)
            return OptionalCheckedState();

        const auto nodeResource = node_view::helpers::getResource(node);
        const auto id = nodeResource ? nodeResource->getId() : QnUuid();
        const auto checkedState = node_view::helpers::checkedState(node, *columns.begin());
        if (id.isNull() || !ids.contains(id) || checkedState == targetState)
            return checkedState;

        addCheckStateChangeToPatch(patch, node->path(), columns, targetState);
        return targetState;
    }

    NodeList allSiblingsCheckNodes;
    OptionalCheckedState cumulativeState;
    for (const auto child: node->children())
    {
        if (node_view::helpers::isAllSiblingsCheckNode(child))
        {
            allSiblingsCheckNodes.append(child);
            continue;
        }

        const auto childCheckState = setResourcesCheckedInternal(
            patch, columns, child, ids, targetState);
        if (!childCheckState)
            continue;

        if (!cumulativeState)
            cumulativeState = *childCheckState;
        else if (*cumulativeState != *childCheckState)
            cumulativeState = Qt::PartiallyChecked;
    }

    if (!cumulativeState && checkableNode)
        return node_view::helpers::checkedState(node, *columns.begin());

    if (cumulativeState)
    {
        for (const auto allCheckNode: allSiblingsCheckNodes)
            addCheckStateChangeToPatch(patch, allCheckNode->path(), columns, *cumulativeState);
    }

    if (!checkableNode)
        return OptionalCheckedState();

    addCheckStateChangeToPatch(patch, node->path(), columns, *cumulativeState);
    return cumulativeState;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

NodeViewStatePatch ResourceNodeViewStateReducer::getLeafResourcesCheckedPatch(
    const ColumnsSet& columns,
    const NodeViewState& state,
    const QnResourceList& resources)
{
    NodeViewStatePatch patch;
    ResourceIdSet ids;
    for (const auto resource: resources)
        ids.insert(resource->getId());

    setResourcesCheckedInternal(patch, columns, state.rootNode, ids, Qt::Checked);
    return patch;
}

} // namespace desktop
} // namespace client
} // namespace nx
