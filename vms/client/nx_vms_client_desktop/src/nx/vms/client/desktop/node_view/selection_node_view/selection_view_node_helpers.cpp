#include "selection_view_node_helpers.h"

#include "selection_node_view_constants.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_data.h"
#include "../details/node/view_node_data_builder.h"

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

int selectedChildrenCount(const NodePtr& node)
{
    return node ? node->property(selectedChildrenCountProperty).toInt() : 0;
}

void setSelectedChildrenCount(ViewNodeData& data, int count)
{
    data.setProperty(selectedChildrenCountProperty, count);
}

} // namespace node_view
} // namespace nx::vms::client::desktop
