// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include "abstract_state_view_filter.h"

namespace nx::vms::client::core::analytics::taxonomy {

/**
 * Composite filters.
 */
class CompositeFilter: public AbstractStateViewFilter
{
public:
    CompositeFilter(
        const std::vector<AbstractStateViewFilter*> filters,
        QObject* parent = nullptr);

    virtual QString id() const override;
    virtual QString name() const override;

    virtual bool matches(
        const nx::analytics::taxonomy::AbstractObjectType* objectType) const override;

    virtual bool matches(
        const nx::analytics::taxonomy::AbstractAttribute* attribute) const override;

private:
    std::vector<AbstractStateViewFilter*> m_filters;
};

} // namespace nx::vms::client::core::analytics::taxonomy
