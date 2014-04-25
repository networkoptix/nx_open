#ifndef QN_API_CONVERSION_FUNCTIONS_H
#define QN_API_CONVERSION_FUNCTIONS_H

#include "api_globals.h"

#include <core/resource/resource_fwd.h>
#include <business/business_fwd.h>

namespace ec2 {

    void fromApiToResource(const ApiBusinessRuleData &src, QnBusinessEventRulePtr &dst, QnResourcePool *resourcePool);
    void fromResourceToApi(const QnBusinessEventRulePtr &src, ApiBusinessRuleData &dst);
    void fromApiToResourceList(const ApiBusinessRuleDataList &src, QnBusinessEventRuleList &dst, QnResourcePool *resourcePool);
    void fromResourceListToApi(const QnBusinessEventRuleList &src, ApiBusinessRuleDataList &dst);

    void fromResourceToApi(const QnAbstractBusinessActionPtr &src, ApiBusinessActionData &dst);
    void fromApiToResource(const ApiBusinessActionData &src, QnAbstractBusinessActionPtr &dst, QnResourcePool *resourcePool);

    void fromResourceToApi(const QnScheduleTask &src, ApiScheduleTaskData &dst);
    void fromApiToResource(const ApiScheduleTaskData &src, QnScheduleTask &dst, const QnId &resourceId);

    void fromApiToResource(const ApiCameraData &src, QnVirtualCameraResourcePtr &dst);
    void fromResourceToApi(const QnVirtualCameraResourcePtr &src, ApiCameraData &dst);

    void fromApiToResourceList(const ApiCameraDataList &src, QnResourceList &dst, QnResourceFactory *factory);
    void fromApiToResourceList(const ApiCameraDataList &src, QnVirtualCameraResourceList &dst, QnResourceFactory *factory);
    void fromResourceListToApi(const QnVirtualCameraResourceList &src, ApiCameraDataList &dst);

} // namespace ec2

#endif // QN_API_CONVERSION_FUNCTIONS_H
