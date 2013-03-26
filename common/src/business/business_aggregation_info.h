#ifndef BUSINESS_AGGREGATION_INFO_H
#define BUSINESS_AGGREGATION_INFO_H

#include <business/business_logic_common.h>

struct QnInfoDetail {
    QnInfoDetail(): count(0) {}

    QnBusinessParams runtimeParams;
    int count;
};

class QnBusinessAggregationInfo
{
public:
    QnBusinessAggregationInfo();

    void append(const QnBusinessParams& runtimeParams);
    int totalCount() const;
    void clear();

    QList<QnInfoDetail> toList() const;
private:
    QMap<QString, QnInfoDetail> m_details;
};

#endif // BUSINESS_AGGREGATION_INFO_H
