#pragma once

#include <vector>

namespace nx::vms_server_plugins::analytics::hikvision {

struct Point
{
    int x = 0;
    int y = 0;
    bool operator==(Point other) const { return x == other.x && y == other.y; }
};

using Points = std::vector<Point>;

struct Region
{
    int id = 0;
    Points points;
};

using Regions = std::vector<Region>;

} // namespace nx::vms_server_plugins::analytics::hikvision
