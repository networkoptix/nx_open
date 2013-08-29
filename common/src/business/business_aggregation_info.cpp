#include "business_aggregation_info.h"

QnBusinessAggregationInfo::QnBusinessAggregationInfo() {
}


int QnBusinessAggregationInfo::totalCount() const {
    int result = 0;
    QMap<QString, QnInfoDetail>::const_iterator itr = m_details.constBegin();
    while (itr != m_details.end())
    {
        result += itr.value().count;
        ++itr;
    }
    return result;
}

void QnBusinessAggregationInfo::clear() {
    m_details.clear();
}

bool QnBusinessAggregationInfo::isEmpty() const {
    return m_details.isEmpty();
}

void QnBusinessAggregationInfo::append(const QnBusinessEventParameters &runtimeParams) {
    QString key = runtimeParams.getParamsKey();
    QnInfoDetail& info = m_details[key];
    if (info.count == 0) //not initialized
        info.runtimeParams = runtimeParams; //timestamp of first event is stored here
    info.count++;
}

QList<QnInfoDetail> QnBusinessAggregationInfo::toList() const {
    return m_details.values();
}
