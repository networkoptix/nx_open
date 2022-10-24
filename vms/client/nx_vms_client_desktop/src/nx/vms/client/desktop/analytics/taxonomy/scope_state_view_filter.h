// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <set>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/analytics/taxonomy/abstract_state_view_filter.h>

namespace nx::analytics::taxonomy { class AbstractEngine; }

namespace nx::vms::client::desktop::analytics::taxonomy {

/**
 * Filters Object types by the taxonomy Engine and devices.
 */
class ScopeStateViewFilter: public AbstractStateViewFilter
{
public:
    ScopeStateViewFilter(
        nx::analytics::taxonomy::AbstractEngine* engine = nullptr,
        const std::optional<std::set<QnUuid>>& devices = {},
        QObject* parent = nullptr);

    virtual ~ScopeStateViewFilter() override;

    /** Id of the filter is id of the corresponding Engine. */
    virtual QString id() const override;

    /** Name of the filter is the name of the corresponding Engine. */
    virtual QString name() const override;

    /**
     * @return True if the Object type is provided by the taxonomy Engine of the filter (i.e.
     * the Engine is in the list of the Object type scopes).
     */
    virtual bool matches(
        const nx::analytics::taxonomy::AbstractObjectType* objectType) const override;

    /** @return True if attribute belongs to an Object type that matches the filter. */
    virtual bool matches(
        const nx::analytics::taxonomy::AbstractAttribute* attribute) const override;

    nx::analytics::taxonomy::AbstractEngine* engine() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
