// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_filter_field.h"

namespace nx::vms::rules {

bool ResourceFilterEventField::match(const QVariant& value) const
{
    return acceptAll()
        ? true
        : ids().contains(value.value<nx::Uuid>());
}

QVariant ResourceFilterActionField::build(const AggregatedEventPtr& /*eventData*/) const
{
    return QVariant::fromValue(UuidSelection{
        .ids = ids(),
        .all = acceptAll(),
    });
}

void ResourceFilterActionField::setSelection(const UuidSelection& selection)
{
    setAcceptAll(selection.all);
    setIds(selection.ids);
}

} // namespace nx::vms::rules
