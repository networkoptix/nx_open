// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_object_attributes_field.h"

#include <analytics/db/text_search_utils.h>

namespace nx::vms::rules {

bool AnalyticsObjectAttributesField::match(const QVariant& value) const
{
    nx::analytics::db::TextMatcher textMatcher;
    textMatcher.parse(m_value);
    textMatcher.matchAttributes(value.value<nx::common::metadata::Attributes>());

    return textMatcher.matched();
}

} // namespace nx::vms::rules
