// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute_condition_state_view_filter.h"

#include <analytics/db/text_search_utils.h>
#include <nx/analytics/taxonomy/abstract_attribute.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::client::core::analytics::taxonomy {

struct AttributeConditionStateViewFilter::Private
{
    const AbstractStateViewFilter* const baseFilter;
    nx::common::metadata::Attributes attributeValues;
};

AttributeConditionStateViewFilter::AttributeConditionStateViewFilter(
    const AbstractStateViewFilter* baseFilter,
    nx::common::metadata::Attributes attributeValues,
    QObject* parent)
    :
    AbstractStateViewFilter(parent),
    d(new Private{.baseFilter = baseFilter, .attributeValues = std::move(attributeValues)})
{
}

AttributeConditionStateViewFilter::~AttributeConditionStateViewFilter()
{
    // Required here for a forward-declared scoped pointer destruction.
}

QString AttributeConditionStateViewFilter::id() const
{
    QString id;
    if (d->baseFilter)
        id = d->baseFilter->id() + "$";

    return id + QJson::serialized(d->attributeValues);
}

QString AttributeConditionStateViewFilter::name() const
{
    return QString();
}

bool AttributeConditionStateViewFilter::matches(
    const nx::analytics::taxonomy::AbstractObjectType* objectType) const
{
    if (!d->baseFilter)
        return true;

    return d->baseFilter->matches(objectType);
}

bool AttributeConditionStateViewFilter::matches(
    const nx::analytics::taxonomy::AbstractAttribute* attribute) const
{
    if (!NX_ASSERT(attribute))
        return false;

    if (d->baseFilter && !d->baseFilter->matches(attribute))
        return false;

    const QString condition = attribute->condition();
    if (condition.isEmpty())
        return true;

    nx::analytics::db::TextMatcher textMatcher;
    textMatcher.parse(condition);
    return textMatcher.matchAttributes(d->attributeValues);
}

} // namespace nx::vms::client::core::analytics::taxonomy
