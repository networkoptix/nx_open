#include "node_type.h"

namespace nx::vms::client::desktop {

bool isSeparatorNode(ResourceTreeNodeType t)
{
    return t == ResourceTreeNodeType::separator
        || t == ResourceTreeNodeType::localSeparator;
}

} // namespace nx::vms::client::desktop
