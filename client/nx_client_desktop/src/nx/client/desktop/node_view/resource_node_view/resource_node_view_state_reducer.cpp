#include "resource_node_view_state_reducer.h"

#include "resource_view_node_helpers.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_helpers.h"
#include "../selection_node_view/selection_view_node_helpers.h"
#include "../selection_node_view/selection_node_view_state_reducer.h"

#include <core/resource/resource.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

details::NodeViewStatePatch ResourceNodeViewStateReducer::getLeafResourcesCheckedPatch(
    const details::ColumnsSet& selectionColumns,
    const details::NodeViewState& state,
    const QnResourceList& resources)
{
    if (selectionColumns.isEmpty())
        return details::NodeViewStatePatch();

    details::NodeViewStatePatch patch;

    QSet<QnUuid> ids;
    for (const auto resource: resources)
        ids.insert(resource->getId());

    const int anySelectionColumn = *selectionColumns.begin();
    details::forEachLeaf(state.rootNode,
        [&patch, ids, selectionColumns, anySelectionColumn, &state](const details::NodePtr& node)
        {
            if (details::checkedState(node, anySelectionColumn) == Qt::Checked)
                return; // Node is checked already.

            const auto resource = getResource(node);
            if (!resource || !ids.contains(resource->getId()))
                return;

            const auto selectionPatch =
                SelectionNodeViewStateReducer::setNodeSelected(state, selectionColumns,
                node->path(), anySelectionColumn, Qt::Checked);

            patch.appendPatchSteps(selectionPatch);
        });

    return patch;
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
