#ifndef QN_TIME_PERIOD_STORAGE_H
#define QN_TIME_PERIOD_STORAGE_H

#include <recording/time_period.h>
#include <recording/time_period_list.h>

/**
 * Class for collecting, aggregating and providing time period lists.
 */
class QnTimePeriodStorage {
public: 
    QnTimePeriodStorage(): m_aggregationMSecs(0) {}
    QnTimePeriodList periods(Qn::TimePeriodContent type) const;
    QnTimePeriodList aggregated(Qn::TimePeriodContent type) const;
    void setPeriods( Qn::TimePeriodContent type, const QnTimePeriodList &timePeriods );
    void setAggregationMSecs(int value);
    int aggregationMSecs() const;
protected:
    void updateAggregated(Qn::TimePeriodContent type);    
private:
    QnTimePeriodList m_normalPeriods[Qn::TimePeriodContentCount];
    QnTimePeriodList m_aggregatedPeriods[Qn::TimePeriodContentCount];
    int m_aggregationMSecs;
};


#endif // QN_TIME_PERIOD_STORAGE_H
