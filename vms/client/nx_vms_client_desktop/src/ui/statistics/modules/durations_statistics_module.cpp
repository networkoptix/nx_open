// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#include "durations_statistics_module.h"

#include <statistics/base/metrics_container.h>

#include <ui/statistics/modules/private/session_uptime_metric.h>
#include <ui/statistics/modules/private/app_active_time_metric.h>
#include <ui/statistics/modules/private/udt_internet_traffic_metric.h>

QnDurationStatisticsModule::QnDurationStatisticsModule(QObject *parent)
    : base_type(parent)
    , m_metrics(new QnMetricsContainer())
{
    m_metrics->addMetric<SessionUptimeMetric>("session_ms");
    m_metrics->addMetric<AppActiveTimeMetric>("active_ms");
    m_metrics->addMetric<UdtInternetTrafficMetric>("udtInternetTraffic_bytes");
}

QnDurationStatisticsModule::~QnDurationStatisticsModule()
{}

QnStatisticValuesHash QnDurationStatisticsModule::values() const
{
    return m_metrics->values();
}

void QnDurationStatisticsModule::reset()
{
    m_metrics->reset();
}
