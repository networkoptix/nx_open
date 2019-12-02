#pragma once

#include <QtCore/QSharedPointer>

#include "node/view_node_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view::details {

struct NodeViewState;
struct NodeViewStatePatch;

class NodeViewModel;
class NodeViewStore;

using NodeViewStorePtr = QSharedPointer<NodeViewStore>;

} // namespace node_view::details
} // namespace nx::vms::client::desktop
