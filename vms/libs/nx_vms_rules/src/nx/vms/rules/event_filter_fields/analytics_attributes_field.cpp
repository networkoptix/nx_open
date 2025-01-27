// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_attributes_field.h"

#include <analytics/db/text_search_utils.h>

namespace nx::vms::rules {

bool AnalyticsAttributesField::match(const QVariant& eventValue) const
{
    const auto text = value();
    if (text.isEmpty())
        return true;

    const auto attributes = eventValue.value<nx::common::metadata::Attributes>();
    if (attributes.empty())
        return false;

    nx::analytics::db::TextMatcher textMatcher(text);
    return textMatcher.matchAttributes(attributes);
}

} // namespace nx::vms::rules
