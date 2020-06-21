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
    bool isFullFrame() const
    {
        return id == 1
            && points.size() == 4
            && points[0] == Point{ 0, 0 }
            && points[1] == Point{ 0, 1000 }
            && points[2] == Point{ 1000, 1000 }
            && points[3] == Point{ 1000, 0 };
    }
};

using Regions = std::vector<Region>;

} // namespace nx::vms_server_plugins::analytics::hikvision
