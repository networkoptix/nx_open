#pragma once

namespace nx {
namespace client {
namespace desktop {

class ViewNodePath;
class NodeViewStatePatch;

namespace helpers {

void addCheckStateChangeToPatch(
    NodeViewStatePatch& patch,
    const ViewNodePath& path,
    Qt::CheckState state);

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx
