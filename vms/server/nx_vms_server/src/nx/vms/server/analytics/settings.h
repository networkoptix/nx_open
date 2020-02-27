#pragma once

#include <QtCore/QJsonObject>

namespace nx::vms::server::analytics {

struct Settings
{
    QJsonObject model;
    QJsonObject values;
};

} // namespace nx::vms::server::analytics
