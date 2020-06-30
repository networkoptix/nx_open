#pragma once

#include <vector>

#include <nx/sdk/analytics/point.h>

namespace nx::vms_server_plugins::analytics::vivotek {

struct Polygon: std::vector<nx::sdk::analytics::Point>
{
    using std::vector<nx::sdk::analytics::Point>::vector;
    using std::vector<nx::sdk::analytics::Point>::operator=;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
