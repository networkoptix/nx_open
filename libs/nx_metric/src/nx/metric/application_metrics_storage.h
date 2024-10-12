// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/metric/base_metrics_storage.h>
#include <nx/utils/impl_ptr.h>

namespace nx::metric {

struct ApplicationMetricsStorage: ParameterSet
{
    NX_METRICS_ADD(std::atomic<qint64>, totalServerRequests, "Total number of requests to server");
};

} // namespace nx::metric::
