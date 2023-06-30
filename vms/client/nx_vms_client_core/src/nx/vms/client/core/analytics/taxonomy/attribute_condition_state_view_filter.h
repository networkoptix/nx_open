// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/analytics/taxonomy/abstract_state_view_filter.h>

namespace nx::vms::client::core::analytics::taxonomy {

class NX_VMS_CLIENT_CORE_API AttributeConditionStateViewFilter: public AbstractStateViewFilter
{
public:
    AttributeConditionStateViewFilter(
        const AbstractStateViewFilter* baseFilter,
        nx::common::metadata::Attributes attributeValues,
        QObject* parent = nullptr);

    virtual ~AttributeConditionStateViewFilter() override;

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
