// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "drawing_items.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop::integrations::internal::roi {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Box, (json), Box_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Polygon, (json), Polygon_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Line, (json), Line_Fields, (brief, true))

namespace {

bool pointsAreValid(const Points& points)
{
    return std::all_of(points.cbegin(), points.cend(),
        [] (const Point& point){ return point.size() == 2; });
}

} // namespace

bool Item::isValid() const
{
    return pointsAreValid(points);
}

bool Box::isValid() const
{
    return points.size() == 2 && Item::isValid();
}

bool Polygon::isValid() const
{
    return points.size() > 2 && Item::isValid();
}

bool Line::isValid() const
{
    return points.size() == 2 && Item::isValid();
}

} // namespace nx::vms::client::desktop::integrations::internal::roi
