// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "composite_state_view_filter.h"

namespace nx::vms::client::core::analytics::taxonomy {

CompositeFilter::CompositeFilter(
    const std::vector<AbstractStateViewFilter*> filters,
    QObject* parent)
    :
    m_filters(filters)
{
}

QString CompositeFilter::id() const
{
    QStringList ids;
    for (AbstractStateViewFilter* filter: m_filters)
        ids.push_back(filter->id());

    return ids.join("&");
}

QString CompositeFilter::name() const
{
    QStringList names;
    for (AbstractStateViewFilter* filter: m_filters)
        names.push_back(filter->name());

    return names.join("&");
}

bool CompositeFilter::matches(const nx::analytics::taxonomy::AbstractObjectType* objectType) const
{
    return std::all_of(m_filters.begin(), m_filters.end(),
        [&](AbstractStateViewFilter* filter) { return filter->matches(objectType); });
}

bool CompositeFilter::matches(const nx::analytics::taxonomy::AbstractAttribute* attribute) const
{
    return std::all_of(m_filters.begin(), m_filters.end(),
        [&](AbstractStateViewFilter* filter) { return filter->matches(attribute); });
}

} // namespace nx::vms::client::core::analytics::taxonomy
