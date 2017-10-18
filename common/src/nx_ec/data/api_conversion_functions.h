#ifndef QN_API_CONVERSION_FUNCTIONS_H
#define QN_API_CONVERSION_FUNCTIONS_H

#include "api_globals.h"

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <nx/vms/event/event_fwd.h>
#include <nx_ec/ec_api_fwd.h>
#include <utils/email/email_fwd.h>
#include <utils/common/ldap_fwd.h>

class SocketAddress;
class QnCameraUserAttributePool;
class QnCommonModule;

namespace ec2
{
    void fromApiToResource(const ApiBusinessRuleData& src, nx::vms::event::RulePtr& dst);
    void fromResourceToApi(const nx::vms::event::RulePtr& src, ApiBusinessRuleData& dst);
    void fromApiToResourceList(const ApiBusinessRuleDataList& src, nx::vms::event::RuleList& dst);
    void fromResourceListToApi(const nx::vms::event::RuleList& src, ApiBusinessRuleDataList& dst);

    void fromResourceToApi(const nx::vms::event::AbstractActionPtr& src, ApiBusinessActionData& dst);
    void fromApiToResource(const ApiBusinessActionData& src, nx::vms::event::AbstractActionPtr& dst);

    void fromApiToResource(
        const ApiCameraData& src,
        QnVirtualCameraResourcePtr& dst);
    void fromResourceToApi(
        const QnVirtualCameraResourcePtr& src,
        ApiCameraData& dst);
    void fromResourceListToApi(
        const QnVirtualCameraResourceList& src,
        ApiCameraDataList& dst);

    void fromResourceToApi(const QnScheduleTask& src, ApiScheduleTaskData& dst);
    void fromApiToResource(const ApiScheduleTaskData& src, QnScheduleTask& dst, const QnUuid& resourceId);

    void fromApiToResource(const ApiCameraAttributesData& src, const QnCameraUserAttributesPtr& dst);
    void fromResourceToApi(const QnCameraUserAttributesPtr& src, ApiCameraAttributesData& dst);
    void fromApiToResourceList(const ApiCameraAttributesDataList& src, QnCameraUserAttributesList& dst);
    void fromResourceListToApi(const QnCameraUserAttributesList& src, ApiCameraAttributesDataList& dst);

    void fromApiToResource(
        const ApiCameraDataEx& src,
        QnVirtualCameraResourcePtr& dst,
        QnCameraUserAttributePool* attributesPool);
    void fromResourceToApi(
        const QnVirtualCameraResourcePtr& src,
        ApiCameraDataEx& dst,
        QnCameraUserAttributePool* attributesPool);
    void fromResourceListToApi(
        const QnVirtualCameraResourceList& src,
        ApiCameraDataExList& dst,
        QnCameraUserAttributePool* attributesPool);

    void fromResourceToApi(const QnEmailSettings& src, ApiEmailSettingsData& dst);
    void fromApiToResource(const ApiEmailSettingsData& src, QnEmailSettings& dst);

    void fromApiToResource(const ApiLayoutItemData& src, QnLayoutItemData& dst);
    void fromResourceToApi(const QnLayoutItemData& src, ApiLayoutItemData& dst);

    void fromApiToResource(const ApiLayoutData& src, QnLayoutResourcePtr& dst);
    void fromResourceToApi(const QnLayoutResourcePtr& src, ApiLayoutData& dst);
    void fromApiToResourceList(const ApiLayoutDataList& src, QnResourceList& dst);
    void fromApiToResourceList(const ApiLayoutDataList& src, QnLayoutResourceList& dst);
    void fromResourceListToApi(const QnLayoutResourceList& src, ApiLayoutDataList& dst);

    void fromResourceToApi(const QnLicensePtr& src, ApiLicenseData& dst);
    void fromApiToResource(const ApiLicenseData& src, QnLicensePtr& dst);
    void fromResourceToApi(const QnLicensePtr& src, ApiDetailedLicenseData& dst);
    void fromResourceListToApi(const QnLicenseList& src, ApiLicenseDataList& dst);
    void fromApiToResourceList(const ApiLicenseDataList& src, QnLicenseList& dst);

    void fromResourceToApi(const QnStorageResourcePtr& src, ApiStorageData& dst);
    void fromApiToResource(const ApiStorageData& src, QnStorageResourcePtr& dst);
    void fromResourceListToApi(const QnStorageResourceList& src, ApiStorageDataList& dst);

    void fromResourceToApi(const QnMediaServerResourcePtr& src, ApiMediaServerData& dst);
    void fromApiToResource(const ApiMediaServerData& src, QnMediaServerResourcePtr& dst);
    void fromApiToResourceList(const ApiMediaServerDataList& src, QnResourceList& dst, QnCommonModule* commonModule);
    void fromApiToResourceList(const ApiMediaServerDataList& src, QnMediaServerResourceList& dst, QnCommonModule* commonModule);
    void fromResourceToApi(const QnMediaServerUserAttributesPtr& src, ApiMediaServerUserAttributesData& dst);
    void fromApiToResource(const ApiMediaServerUserAttributesData& src, QnMediaServerUserAttributesPtr& dst);
    void fromApiToResourceList(const ApiMediaServerUserAttributesDataList& src, QnMediaServerUserAttributesList& dst);
    void fromResourceListToApi(const QnMediaServerUserAttributesList& src, ApiMediaServerUserAttributesDataList& dst);

    void fromResourceToApi(const QnResourcePtr& src, ApiResourceData& dst);
    void fromApiToResource(const ApiResourceData& src, QnResource* dst);

    void fromApiToResource(const ApiResourceTypeData& src, QnResourceTypePtr& dst);
    void fromApiToResourceList(const ApiResourceTypeDataList& src, QnResourceTypeList& dst);

    QnUserResourcePtr fromApiToResource(const ApiUserData& src, QnCommonModule* commonModule = nullptr);
    void fromApiToResource(const ApiUserData& src, QnUserResourcePtr& dst);
    void fromResourceToApi(const QnUserResourcePtr& resource, ApiUserData& data);
    void fromApiToResourceList(const ApiUserDataList& src, QnResourceList& dst);
    void fromApiToResourceList(const ApiUserDataList& src, QnUserResourceList& dst);

    void fromApiToResource(const ApiVideowallData& src, QnVideoWallResourcePtr& dst);
    void fromResourceToApi(const QnVideoWallResourcePtr& src, ApiVideowallData& dst);
    void fromApiToResourceList(const ApiVideowallDataList& src, QnResourceList& dst);
    void fromApiToResourceList(const ApiVideowallDataList& src, QnVideoWallResourceList& dst);

    void fromApiToResource(const ApiVideowallControlMessageData& data, QnVideoWallControlMessage& message);
    void fromResourceToApi(const QnVideoWallControlMessage& message, ApiVideowallControlMessageData& data);

    void fromApiToResource(const ApiWebPageData& src, QnWebPageResourcePtr& dst);
    void fromResourceToApi(const QnWebPageResourcePtr& src, ApiWebPageData& dst);
    void fromApiToResourceList(const ApiWebPageDataList& src, QnResourceList& dst);
    void fromApiToResourceList(const ApiWebPageDataList& src, QnWebPageResourceList& dst);

    void deserializeNetAddrList(const QString& source, QList<SocketAddress>& target, int defaultPort);
} // namespace ec2

#endif // QN_API_CONVERSION_FUNCTIONS_H
