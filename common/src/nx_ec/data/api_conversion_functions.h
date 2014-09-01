#ifndef QN_API_CONVERSION_FUNCTIONS_H
#define QN_API_CONVERSION_FUNCTIONS_H

#include "api_globals.h"

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <business/business_fwd.h>
#include <nx_ec/ec_api_fwd.h>

namespace ec2 {

    void fromApiToResource(const ApiBusinessRuleData &src, QnBusinessEventRulePtr &dst, QnResourcePool *resourcePool);
    void fromResourceToApi(const QnBusinessEventRulePtr &src, ApiBusinessRuleData &dst);
    void fromApiToResourceList(const ApiBusinessRuleDataList &src, QnBusinessEventRuleList &dst, QnResourcePool *resourcePool);
    void fromResourceListToApi(const QnBusinessEventRuleList &src, ApiBusinessRuleDataList &dst);

    void fromResourceToApi(const QnAbstractBusinessActionPtr &src, ApiBusinessActionData &dst);
    void fromApiToResource(const ApiBusinessActionData &src, QnAbstractBusinessActionPtr &dst, QnResourcePool *resourcePool);

    void fromResourceToApi(const QnScheduleTask &src, ApiScheduleTaskData &dst);
    void fromApiToResource(const ApiScheduleTaskData &src, QnScheduleTask &dst, const QUuid &resourceId);

    void fromApiToResource(const ApiCameraData &src, QnVirtualCameraResourcePtr &dst);
    void fromResourceToApi(const QnVirtualCameraResourcePtr &src, ApiCameraData &dst);
    void fromApiToResourceList(const ApiCameraDataList &src, QnResourceList &dst, QnResourceFactory *factory);
    void fromApiToResourceList(const ApiCameraDataList &src, QnVirtualCameraResourceList &dst, QnResourceFactory *factory);
    void fromResourceListToApi(const QnVirtualCameraResourceList &src, ApiCameraDataList &dst);

    void fromResourceToApi(const QnCameraHistoryItem &src, ApiCameraServerItemData &dst);
    void fromApiToResource(const ApiCameraServerItemData &src, QnCameraHistoryItem &dst);
    void fromApiToResourceList(const ApiCameraServerItemDataList &src, QnCameraHistoryList &dst);

    void fromResourceToApi(const QnEmail::Settings &src, ApiEmailSettingsData &dst);
    void fromApiToResource(const ApiEmailSettingsData &src, QnEmail::Settings &dst);

    void fromApiToResourceList(const ApiFullInfoData &src, QnFullResourceData &dst, const ResourceContext &ctx);

    void fromApiToResource(const ApiLayoutItemData &src, QnLayoutItemData &dst);
    void fromResourceToApi(const QnLayoutItemData &src, ApiLayoutItemData &dst);

    void fromApiToResource(const ApiLayoutData &src, QnLayoutResourcePtr &dst);
    void fromResourceToApi(const QnLayoutResourcePtr &src, ApiLayoutData &dst);
    void fromApiToResourceList(const ApiLayoutDataList &src, QnResourceList &dst);
    void fromApiToResourceList(const ApiLayoutDataList &src, QnLayoutResourceList &dst);
    void fromResourceListToApi(const QnLayoutResourceList &src, ApiLayoutDataList &dst);

    void fromResourceToApi(const QnLicensePtr &src, ApiLicenseData &dst);
    void fromApiToResource(const ApiLicenseData &src, QnLicensePtr &dst);
    void fromResourceToApi(const QnLicensePtr &src, ApiDetailedLicenseData &dst);
    void fromResourceListToApi(const QnLicenseList &src, ApiLicenseDataList &dst);
    void fromApiToResourceList(const ApiLicenseDataList &src, QnLicenseList &dst);

    void fromResourceToApi(const QnAbstractStorageResourcePtr &src, ApiStorageData &dst);
    void fromApiToResource(const ApiStorageData &src, QnAbstractStorageResourcePtr &dst);

    void fromResourceToApi(const QnMediaServerResourcePtr& src, ApiMediaServerData &dst);
    void fromApiToResource(const ApiMediaServerData &src, QnMediaServerResourcePtr &dst, const ResourceContext &ctx);
    void fromApiToResourceList(const ApiMediaServerDataList &src, QnResourceList &dst, const ResourceContext &ctx);
    void fromApiToResourceList(const ApiMediaServerDataList &src, QnMediaServerResourceList &dst, const ResourceContext &ctx);

    void fromResourceToApi(const QnResourcePtr &src, ApiResourceData &dst);
    void fromApiToResource(const ApiResourceData &src, QnResourcePtr &dst);
    void fromApiToResourceList(const ApiResourceDataList &src, QnResourceList &dst, QnResourceFactory *factory);

    void fromResourceListToApi(const QnKvPairList &src, ApiResourceParamDataList &dst);
    void fromApiToResourceList(const ApiResourceParamDataList &src, QnKvPairList &dst);

    void fromApiToResource(const ApiPropertyTypeData &src, QnParamTypePtr &dst);
    
    void fromApiToResource(const ApiResourceTypeData &src, QnResourceTypePtr &dst);
    void fromApiToResourceList(const ApiResourceTypeDataList &src, QnResourceTypeList &dst);

    void fromApiToResource(const ApiUserData &src, QnUserResourcePtr &dst);
    void fromResourceToApi(const QnUserResourcePtr &resource, ApiUserData &data);
    void fromApiToResourceList(const ApiUserDataList &src, QnResourceList &dst);
    void fromApiToResourceList(const ApiUserDataList &src, QnUserResourceList &dst);

    void fromApiToResource(const ApiVideowallData &src, QnVideoWallResourcePtr &dst);
    void fromResourceToApi(const QnVideoWallResourcePtr &src, ApiVideowallData &dst);
    void fromApiToResourceList(const ApiVideowallDataList &src, QnResourceList &dst);
    void fromApiToResourceList(const ApiVideowallDataList &src, QnVideoWallResourceList &dst);

    void fromApiToResource(const ApiVideowallControlMessageData &data, QnVideoWallControlMessage &message);
    void fromResourceToApi(const QnVideoWallControlMessage &message, ApiVideowallControlMessageData &data);

    void fromApiToResource(const ApiCameraBookmarkTagDataList &data, QnCameraBookmarkTags &tags);
    void fromResourceToApi(const QnCameraBookmarkTags &tags, ApiCameraBookmarkTagDataList &data);
    

} // namespace ec2

#endif // QN_API_CONVERSION_FUNCTIONS_H
