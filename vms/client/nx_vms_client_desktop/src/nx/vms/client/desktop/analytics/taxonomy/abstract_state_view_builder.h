// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "abstract_state_view.h"

Q_MOC_INCLUDE("nx/vms/client/desktop/analytics/taxonomy/abstract_state_view_filter.h")

namespace nx::vms::client::desktop::analytics::taxonomy {

class AbstractStateViewFilter;

/**
 * Builds state views and provides different filters to build them.
 */
class NX_VMS_CLIENT_DESKTOP_API AbstractStateViewBuilder: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<nx::vms::client::desktop::analytics::taxonomy::AbstractStateViewFilter*>
        engineFilters READ engineFilters CONSTANT)

public:
    virtual ~AbstractStateViewBuilder() {};

    virtual AbstractStateView* stateView(
        const AbstractStateViewFilter* filter = nullptr) const = 0;

    /**
     * @return Filters that allows to build state views containing only Object types and
     * attributes that are provided/supported by a certain Engine.
     */
    virtual std::vector<AbstractStateViewFilter*> engineFilters() const = 0;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
