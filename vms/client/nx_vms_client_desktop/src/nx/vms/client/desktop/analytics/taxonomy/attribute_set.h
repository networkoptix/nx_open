// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_attribute_set.h"

#include <nx/analytics/taxonomy/abstract_object_type.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

class AbstractStateViewFilter;

class AttributeSet: public AbstractAttributeSet
{
public:
    // AttributeSet doesn't own the filter. The filter should live longer than the AttributeSet.
    AttributeSet(const AbstractStateViewFilter* filter, QObject* parent);

    virtual ~AttributeSet() override;

    std::vector<AbstractAttribute*> attributes() const override;

    void addObjectType(nx::analytics::taxonomy::AbstractObjectType* objectType);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
