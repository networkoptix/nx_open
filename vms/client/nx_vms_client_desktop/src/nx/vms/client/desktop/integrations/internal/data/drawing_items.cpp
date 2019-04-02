#include "drawing_items.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop::integrations::internal::roi {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
	nx::vms::client::desktop::integrations::internal::roi, Direction,
    (Direction::none, "none")
    (Direction::left, "left")
    (Direction::right, "right")
    (Direction::both, "both")
)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
	(Box)(Polygon)(Line),
    (json)(eq),
    _Fields,
    (brief, true)
)

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

