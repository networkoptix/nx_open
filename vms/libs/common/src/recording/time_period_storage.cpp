#include "time_period_storage.h"

QnTimePeriodList QnTimePeriodStorage::periods(Qn::TimePeriodContent type) const
{
    return m_normalPeriods[type];
}

QnTimePeriodList QnTimePeriodStorage::aggregated(Qn::TimePeriodContent type) const
{
    return m_aggregatedPeriods[type];
}

void QnTimePeriodStorage::setPeriods(Qn::TimePeriodContent type, const QnTimePeriodList &timePeriods) 
{
    m_normalPeriods[type] = timePeriods;
    updateAggregated(type);
}

void QnTimePeriodStorage::updateAggregated(Qn::TimePeriodContent type)
{
    if (m_aggregationMSecs)
        m_aggregatedPeriods[type] = QnTimePeriodList::aggregateTimePeriods(m_normalPeriods[type], m_aggregationMSecs);
}

void QnTimePeriodStorage::setAggregationMSecs(int value )
{
    if (value == m_aggregationMSecs)
        return;
    m_aggregationMSecs = value;
    if (m_aggregationMSecs)
        for(int type = 0; type < Qn::TimePeriodContentCount; type++)
            updateAggregated(static_cast<Qn::TimePeriodContent>(type));
}

int QnTimePeriodStorage::aggregationMSecs() const
{
    return m_aggregationMSecs;
}
