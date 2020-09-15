#pragma once

#include <vector>

#include <nx/sdk/analytics/point.h>
#include <nx/sdk/analytics/rect.h>

namespace nx::vms_server_plugins::analytics::vivotek {

struct Polygon: std::vector<nx::sdk::analytics::Point>
{
    using std::vector<nx::sdk::analytics::Point>::vector;
    using std::vector<nx::sdk::analytics::Point>::operator=;

    Polygon clipped(const Polygon& clipper) const;
    Polygon simplified(std::size_t desiredSize) const;
};

struct Line: std::vector<nx::sdk::analytics::Point>
{
    using std::vector<nx::sdk::analytics::Point>::vector;
    using std::vector<nx::sdk::analytics::Point>::operator=;

    Line clipped(const Polygon& clipper) const;
    Line simplified(std::size_t desiredSize) const;
    Line subdivided(std::size_t desiredSize) const;
};

struct SizeConstraints
{
    nx::sdk::analytics::Rect min;
    nx::sdk::analytics::Rect max;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
