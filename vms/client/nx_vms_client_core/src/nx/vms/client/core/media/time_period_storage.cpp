// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_period_storage.h"

namespace nx::vms::client::core {

TimePeriodStorage::TimePeriodStorage(QObject* parent):
    AbstractTimePeriodStorage(parent)
{
}

const QnTimePeriodList& TimePeriodStorage::periods(Qn::TimePeriodContent type) const
{
    return m_normalPeriods[type];
}

QnTimePeriodList TimePeriodStorage::aggregated(Qn::TimePeriodContent type) const
{
    return m_aggregatedPeriods[type];
}

void TimePeriodStorage::setPeriods(Qn::TimePeriodContent type, const QnTimePeriodList& timePeriods)
{
    m_normalPeriods[type] = timePeriods;
    updateAggregated(type);
    emit periodsUpdated(type);
}

void TimePeriodStorage::updateAggregated(Qn::TimePeriodContent type)
{
    if (!m_aggregationMSecs)
        return;

    m_aggregatedPeriods[type] =
        QnTimePeriodList::aggregateTimePeriods(m_normalPeriods[type], m_aggregationMSecs);
}

void TimePeriodStorage::setAggregationMSecs(int value)
{
    if (value == m_aggregationMSecs)
        return;

    m_aggregationMSecs = value;

    if (m_aggregationMSecs <= 0)
        return;

    for (int type = 0; type < Qn::TimePeriodContentCount; ++type)
        updateAggregated((Qn::TimePeriodContent) type);
}

int TimePeriodStorage::aggregationMSecs() const
{
    return m_aggregationMSecs;
}

} // namespace nx::vms::client::core
