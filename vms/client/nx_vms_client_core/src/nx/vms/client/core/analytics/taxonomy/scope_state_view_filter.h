// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/analytics/taxonomy/abstract_state_view_filter.h>

namespace nx::analytics::taxonomy { class AbstractEngine; }

namespace nx::vms::client::core::analytics::taxonomy {

/**
 * Object type and attribute scope filter.
 */
class ScopeStateViewFilter: public AbstractStateViewFilter
{
public:
    ScopeStateViewFilter(
        nx::analytics::taxonomy::AbstractEngine* engine = nullptr,
        const std::set<QnUuid>& devices = {},
        QObject* parent = nullptr);

    virtual ~ScopeStateViewFilter() override;

    virtual QString id() const override;
    virtual QString name() const override;

    virtual bool matches(
        const nx::analytics::taxonomy::AbstractObjectType* objectType) const override;

    virtual bool matches(
        const nx::analytics::taxonomy::AbstractAttribute* attribute) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core::analytics::taxonomy
