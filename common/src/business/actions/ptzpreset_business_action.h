#pragma once

#include <business/actions/abstract_business_action.h>
#include <business/business_aggregation_info.h>

#include <core/resource/resource_fwd.h>

class QnPtzPresetBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnPtzPresetBusinessAction(const QnBusinessEventParameters &runtimeParams);
    ~QnPtzPresetBusinessAction() {}
    
    const QnBusinessAggregationInfo &aggregationInfo() const;
    void setAggregationInfo(const QnBusinessAggregationInfo &info);
private:
    QnBusinessAggregationInfo m_aggregationInfo;
};

typedef QSharedPointer<QnPtzPresetBusinessAction> QnPtzPresetBusinessActionPtr;
