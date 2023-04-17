// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "abstract_state_view.h"

namespace nx::analytics::taxonomy { class AbstractState; }

namespace nx::vms::client::desktop::analytics::taxonomy {

class StateView: public AbstractStateView
{
public:
    StateView(std::vector<AbstractNode*> rootNodes, QObject* parent);

    virtual ~StateView() override;

    virtual std::vector<AbstractNode*> rootNodes() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
