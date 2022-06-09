// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/analytics/taxonomy/abstract_state_view_builder.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

class NX_VMS_CLIENT_DESKTOP_API StateViewBuilder: public AbstractStateViewBuilder
{
public:
    StateViewBuilder(std::shared_ptr<nx::analytics::taxonomy::AbstractState> taxonomyState);

    virtual ~StateViewBuilder() override;

    virtual AbstractStateView* stateView(
        const AbstractStateViewFilter* filter = nullptr) const override;

    virtual std::vector<AbstractStateViewFilter*> engineFilters() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
