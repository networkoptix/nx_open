#pragma once

namespace nx {
namespace client {
namespace desktop {

class ViewNodePath;
class NodeViewStatePatch;

namespace details {

void addCheckStateChangeToPatch(
    NodeViewStatePatch& patch,
    const ViewNodePath& path,
    Qt::CheckState state);

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx
