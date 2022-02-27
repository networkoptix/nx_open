// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
