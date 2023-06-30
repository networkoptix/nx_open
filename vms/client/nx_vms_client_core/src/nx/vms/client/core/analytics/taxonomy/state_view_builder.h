// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <QtCore/QObject>

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/utils/impl_ptr.h>

#include "state_view.h"

Q_MOC_INCLUDE("nx/vms/client/core/analytics/taxonomy/abstract_state_view_filter.h")

namespace nx::vms::client::core::analytics::taxonomy {

class AbstractStateViewFilter;

/**
 * Builds state views and provides different filters to build them.
 */
class NX_VMS_CLIENT_CORE_API StateViewBuilder: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<nx::vms::client::core::analytics::taxonomy::AbstractStateViewFilter*>
        engineFilters READ engineFilters CONSTANT)

public:
    StateViewBuilder(std::shared_ptr<nx::analytics::taxonomy::AbstractState> taxonomyState);

    virtual ~StateViewBuilder() override;

    /**
     * Builds a state view using filter. Note that filter must remain valid for the lifetime of
     * StateViewBuilder.
     */
    StateView* stateView(const AbstractStateViewFilter* filter = nullptr) const;

    /**
     * Builds a state view using filter. Note that filter must remain valid for the lifetime of a
     * StateView.
     */
    static StateView* stateView(
        std::shared_ptr<nx::analytics::taxonomy::AbstractState> taxonomyState,
        const AbstractStateViewFilter* filter = nullptr,
        QObject* parent = nullptr);

    /**
     * @return Filters that allows to build state views containing only Object types and
     * attributes that are provided/supported by a certain Engine.
     */
    std::vector<AbstractStateViewFilter*> engineFilters() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core::analytics::taxonomy
