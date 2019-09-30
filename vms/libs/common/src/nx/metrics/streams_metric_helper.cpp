#pragma once

#include "streams_metric_helper.h"
#include <nx/metrics/metrics_storage.h>

namespace nx::vms::metrics {

using namespace nx::vms::api;

StreamMetricHelper::StreamMetricHelper(nx::metrics::Storage* metrics):
    m_metrics(metrics)
{
}

StreamMetricHelper::~StreamMetricHelper()
{
    if (auto metric = getMetric(m_stream))
        metric->fetch_sub(1);
}

void StreamMetricHelper::setStream(StreamIndex stream)
{
    if (auto metric = getMetric(m_stream))
        metric->fetch_sub(1);
    if (auto metric = getMetric(stream))
        metric->fetch_add(1);

    m_stream = stream;
}

std::atomic<qint64>* StreamMetricHelper::getMetric(nx::vms::api::StreamIndex index)
{
    if (!m_metrics)
        return nullptr;
    switch (index)
    {
        case StreamIndex::primary:
            return &m_metrics->primaryStreams();
        case StreamIndex::secondary:
            return &m_metrics->secondaryStreams();
        default:
            return nullptr;
    }
}

} // namespace nx::vms::metrics