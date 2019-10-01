#pragma once

#include <vector>

#include <nx/vms/api/data/analytics_data.h>

namespace nx::vms::server::metrics {

struct PluginMetrics
{
    QString name;
    int numberOfBoundResources = 0;
    int numberOfAliveBoundResources = 0;
};

class IPluginMetricsProvider
{
public:
    virtual ~IPluginMetricsProvider() {}

    virtual std::vector<PluginMetrics> metrics() const = 0;
};

} // namespace nx::vms::server::metrics
