#ifndef BUSINESS_AGGREGATION_INFO_H
#define BUSINESS_AGGREGATION_INFO_H

#include <business/business_logic_common.h>
#include "business_event_parameters.h"

struct QnInfoDetail {
    QnInfoDetail(): count(0) {}

    QnBusinessEventParameters runtimeParams;
    int count;
};

class QnBusinessAggregationInfo
{
public:
    QnBusinessAggregationInfo();

    void append(const QnBusinessEventParameters& runtimeParams);
    int totalCount() const;
    void clear();

    QList<QnInfoDetail> toList() const;
private:
    QMap<QString, QnInfoDetail> m_details;
};

#endif // BUSINESS_AGGREGATION_INFO_H
