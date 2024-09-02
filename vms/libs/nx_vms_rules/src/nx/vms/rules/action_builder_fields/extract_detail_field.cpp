// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "extract_detail_field.h"

#include "../aggregated_event.h"

namespace nx::vms::rules {

ExtractDetailField::ExtractDetailField(
    nx::vms::common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    ActionBuilderField{descriptor},
    SystemContextAware{context}
{
}

QVariant ExtractDetailField::build(const AggregatedEventPtr& event) const
{
    return event->details(systemContext()).value(detailName());
}

QJsonObject ExtractDetailField::openApiDescriptor()
{
    return {};
}

} // namespace nx::vms::rules
