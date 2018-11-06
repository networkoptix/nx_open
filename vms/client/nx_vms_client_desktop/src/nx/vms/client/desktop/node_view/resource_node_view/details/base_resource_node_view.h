#pragma once

#include "../resource_view_node_helpers.h"
#include "../resource_node_view_state_reducer.h"
#include "../../details/node/view_node.h"
#include "../../details/node/view_node_helpers.h"
#include "../../details/node_view_state_patch.h"

#include <core/resource/resource.h>

namespace nx::vms::client::desktop::node_view {

template<typename BaseNodeView>
class BaseResourceNodeView: public BaseNodeView
{
public:
    using BaseNodeView::BaseNodeView;

    void setInvalidLeafResources(const QnUuidSet& resourceIds, bool invalid);
};

template<typename BaseNodeView>
void BaseResourceNodeView<BaseNodeView>::setInvalidLeafResources(
    const QnUuidSet& resourceIds,
    bool invalid)
{
    details::PathList paths;
    const auto rootNode = BaseNodeView::state().rootNode;
    details::forEachLeaf(rootNode,
        [&paths, resourceIds](const details::NodePtr& node)
        {
            const auto resource = getResource(node);
            if (resource && resourceIds.contains(resource->getId()))
                paths.append(node->path());
        });

    BaseNodeView::applyPatch(ResourceNodeViewStateReducer::setInvalidNodes(
        BaseNodeView::state(), paths, invalid));
}

} // namespace nx::vms::client::desktop::node_view
