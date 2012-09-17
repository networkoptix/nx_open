#include "time_period_storage.h"

QnTimePeriodList QnTimePeriodStorage::periods(Qn::TimePeriodRole type) const
{
    return m_normalPeriods[type];
}

QnTimePeriodList QnTimePeriodStorage::aggregated(Qn::TimePeriodRole type) const
{
    return m_aggregatedPeriods[type];
}

void QnTimePeriodStorage::setPeriods( Qn::TimePeriodRole type, const QnTimePeriodList &timePeriods )
{
    m_normalPeriods[type] = timePeriods;
    updateAggregated(type);
}

void QnTimePeriodStorage::updateAggregated(Qn::TimePeriodRole type)
{
    if (m_aggregationMSecs)
        m_aggregatedPeriods[type] = QnTimePeriod::aggregateTimePeriods(m_normalPeriods[type], m_aggregationMSecs);
}

void QnTimePeriodStorage::setAggregationMSecs(int value )
{
    if (value == m_aggregationMSecs)
        return;
    m_aggregationMSecs = value;
    if (m_aggregationMSecs)
        for(int type = 0; type < Qn::TimePeriodRoleCount; type++)
            updateAggregated(static_cast<Qn::TimePeriodRole>(type));
}

int QnTimePeriodStorage::aggregationMSecs() const
{
    return m_aggregationMSecs;
}
