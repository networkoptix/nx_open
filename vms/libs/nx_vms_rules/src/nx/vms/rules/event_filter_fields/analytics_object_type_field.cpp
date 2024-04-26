// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_object_type_field.h"

#include <nx/analytics/taxonomy/helpers.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::rules {

AnalyticsObjectTypeField::AnalyticsObjectTypeField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    SimpleTypeEventField<QString, AnalyticsObjectTypeField>{descriptor},
    SystemContextAware(context)
{
}

bool AnalyticsObjectTypeField::match(const QVariant& eventValue) const
{
    const auto typeId = eventValue.toString();
    if (typeId == value())
        return true;

    return nx::analytics::taxonomy::isBaseType(
        m_context->analyticsTaxonomyState().get(),
        value(),
        typeId);
}

} // namespace nx::vms::rules
