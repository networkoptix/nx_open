#pragma once

#include <vector>

#include <nx/sdk/analytics/point.h>

#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::vivotek {

struct NamedPointSequence: std::vector<nx::sdk::analytics::Point>
{
    QString name;

    using vector<nx::sdk::analytics::Point>::vector;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
