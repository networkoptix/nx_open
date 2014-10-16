#ifndef BUSINESS_AGGREGATION_INFO_H
#define BUSINESS_AGGREGATION_INFO_H

#include <business/business_fwd.h>
#include "business_event_parameters.h"


class QnBusinessAggregationInfo;

class QnInfoDetail {
public:
    QnInfoDetail();
    QnInfoDetail( const QnInfoDetail& right );
    QnInfoDetail( const QnBusinessEventParameters& _runtimeParams, int _count );
    ~QnInfoDetail();

    const QnBusinessEventParameters& runtimeParams() const;
    void setRuntimeParams( const QnBusinessEventParameters& value );
    int count() const;
    void setCount( int value );
    void setSubAggregationData( const QnBusinessAggregationInfo& value );
    const QnBusinessAggregationInfo& subAggregationData() const;

    QnInfoDetail& operator=( const QnInfoDetail& right );

private:
    QnBusinessEventParameters m_runtimeParams;
    int m_count;
    QnBusinessAggregationInfo* m_subAggregationData;
};

class QnBusinessAggregationInfo
{
public:
    QnBusinessAggregationInfo();

    void append(const QnBusinessEventParameters& runtimeParams, const QnBusinessAggregationInfo& subAggregationData = QnBusinessAggregationInfo());
    bool isEmpty() const;
    int totalCount() const;
    void clear();

    QList<QnInfoDetail> toList() const;
private:
    QMap<QString, QnInfoDetail> m_details;
};

#endif // BUSINESS_AGGREGATION_INFO_H
