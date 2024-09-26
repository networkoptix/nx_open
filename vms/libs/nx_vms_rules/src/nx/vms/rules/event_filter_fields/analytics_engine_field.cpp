// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_field.h"

namespace nx::vms::rules {

bool AnalyticsEngineField::match(const QVariant& eventValue) const
{
    return value().isNull() || eventValue.value<nx::Uuid>() == value();
}

QJsonObject AnalyticsEngineField::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor = SimpleTypeEventField::openApiDescriptor(properties);
    descriptor[utils::kDescriptionProperty] =
        "Analytics Engine id. To find the required Analytics Engine id, "
        "refer to the response of the request to <code>/rest/v4/analytics/engines</code>.";
    return descriptor;
}

} // namespace nx::vms::rules
