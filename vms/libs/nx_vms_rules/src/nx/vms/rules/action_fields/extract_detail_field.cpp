// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "extract_detail_field.h"

#include "../aggregated_event.h"
#include "../utils/event_details.h"

namespace nx::vms::rules {

ExtractDetailField::ExtractDetailField(nx::vms::common::SystemContext* context):
    SystemContextAware(context)
{
}

QVariant ExtractDetailField::build(const AggregatedEventPtr& event) const
{
    return event->details(systemContext()).value(detailName());
}

} // namespace nx::vms::rules
