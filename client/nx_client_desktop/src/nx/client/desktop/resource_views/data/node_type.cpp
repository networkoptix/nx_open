#include "node_type.h"

namespace nx {
namespace client {
namespace desktop {

bool isSeparatorNode(ResourceTreeNodeType t)
{
    return t == ResourceTreeNodeType::separator
        || t == ResourceTreeNodeType::localSeparator;
}

} // namespace desktop
} // namespace client
} // namespace nx
