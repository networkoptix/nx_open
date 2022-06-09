// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_view.h"

#include "node.h"

#include <nx/analytics/taxonomy/state.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

struct StateView::Private
{
    std::vector<AbstractNode*> rootNodes;
};

StateView::StateView(std::vector<AbstractNode*> rootNodes, QObject* parent):
    AbstractStateView(parent),
    d(new Private{std::move(rootNodes)})
{
}

StateView::~StateView()
{
    // Required here for forward-declared scoped pointer destruction.
}

std::vector<AbstractNode*> StateView::rootNodes() const
{
    return d->rootNodes;
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
