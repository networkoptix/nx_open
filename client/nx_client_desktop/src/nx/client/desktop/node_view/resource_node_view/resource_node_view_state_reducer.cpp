#include "resource_node_view_state_reducer.h"

#include "resource_view_node_helpers.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_helpers.h"
#include "../node_view/node_view_state_reducer_helpers.h"
#include "../selection_node_view/selection_view_node_helpers.h"

#include <core/resource/resource.h>

namespace {

using namespace nx::client::desktop::node_view;
using namespace nx::client::desktop::node_view::details;

using ResourceIdSet = QSet<QnUuid>;

// Split to the generoc function and move part to the helprs namespace

bool checkable(const ColumnsSet& selectionColumns, const NodePtr& node)
{
    return std::any_of(selectionColumns.begin(), selectionColumns.end(),
        [node](const int column) { return checkable(node, column); });
}

// TODO: move to Selection Node View
OptionalCheckedState setResourcesCheckedInternal(
    NodeViewStatePatch& patch,
    const ColumnsSet& columns,
    const NodePtr& node,
    const ResourceIdSet& ids,
    Qt::CheckState targetState)
{
    const bool checkableNode = checkable(columns, node) && !node->isRoot();
    if (node->isLeaf())
    {
        if (!checkableNode)
            return OptionalCheckedState();

        const auto nodeResource = getResource(node);
        const auto id = nodeResource ? nodeResource->getId() : QnUuid();
        const auto state = checkedState(node, *columns.begin());
        if (id.isNull() || !ids.contains(id) || state == targetState)
            return state;

        addCheckStateChangeToPatch(patch, node->path(), columns, targetState);
        return targetState;
    }

    NodeList allSiblingsCheckNodes;
    OptionalCheckedState cumulativeState;
    for (const auto child: node->children())
    {
        if (isCheckAllNode(child))
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
        return checkedState(node, *columns.begin());

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
