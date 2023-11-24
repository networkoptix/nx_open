// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_object_attributes_field.h"

#include <analytics/db/text_search_utils.h>

namespace nx::vms::rules {

bool AnalyticsObjectAttributesField::match(const QVariant& eventValue) const
{
    nx::analytics::db::TextMatcher textMatcher(value());
    return textMatcher.matchAttributes(eventValue.value<nx::common::metadata::Attributes>());
}

} // namespace nx::vms::rules
