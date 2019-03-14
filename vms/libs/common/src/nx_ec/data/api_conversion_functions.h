#pragma once

#include "api_globals.h"

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <nx/vms/event/event_fwd.h>
#include <nx_ec/ec_api_fwd.h>
#include <utils/email/email_fwd.h>
#include <utils/common/ldap_fwd.h>

#include <nx/vms/api/data_fwd.h>

namespace nx { namespace network { class SocketAddress; }}

class QnCameraUserAttributePool;
class QnCommonModule;

namespace ec2 {

void fromApiToResource(const nx::vms::api::EventRuleData& src, nx::vms::event::RulePtr& dst);
void fromResourceToApi(const nx::vms::event::RulePtr& src, nx::vms::api::EventRuleData& dst);
void fromApiToResourceList(
    const nx::vms::api::EventRuleDataList& src,
    nx::vms::event::RuleList& dst);
void fromResourceListToApi(
    const nx::vms::event::RuleList& src,
    nx::vms::api::EventRuleDataList& dst);

void fromResourceToApi(
    const nx::vms::event::AbstractActionPtr& src,
    nx::vms::api::EventActionData& dst);
void fromApiToResource(
    const nx::vms::api::EventActionData& src,
    nx::vms::event::AbstractActionPtr& dst);

void fromApiToResource(
    const nx::vms::api::CameraData& src,
    QnVirtualCameraResourcePtr& dst);
void fromResourceToApi(
    const QnVirtualCameraResourcePtr& src,
    nx::vms::api::CameraData& dst);
void fromResourceListToApi(
    const QnVirtualCameraResourceList& src,
    nx::vms::api::CameraDataList& dst);

void fromResourceToApi(const QnScheduleTask& src, nx::vms::api::ScheduleTaskData& dst);
void fromApiToResource(const nx::vms::api::ScheduleTaskData& src, QnScheduleTask& dst);

void fromApiToResource(
    const nx::vms::api::CameraAttributesData& src,
    const QnCameraUserAttributesPtr& dst);
void fromResourceToApi(
    const QnCameraUserAttributesPtr& src,
    nx::vms::api::CameraAttributesData& dst);

void fromApiToResourceList(
    const nx::vms::api::CameraAttributesDataList& src,
    QnCameraUserAttributesList& dst);
void fromResourceListToApi(
    const QnCameraUserAttributesList& src,
    nx::vms::api::CameraAttributesDataList& dst);

void fromApiToResource(
    const nx::vms::api::CameraDataEx& src,
    QnVirtualCameraResourcePtr& dst,
    QnCameraUserAttributePool* attributesPool);
void fromResourceToApi(
    const QnVirtualCameraResourcePtr& src,
    nx::vms::api::CameraDataEx& dst,
    QnCameraUserAttributePool* attributesPool);
void fromResourceListToApi(
    const QnVirtualCameraResourceList& src,
    nx::vms::api::CameraDataExList& dst,
    QnCameraUserAttributePool* attributesPool);

void fromResourceToApi(const QnEmailSettings& src, nx::vms::api::EmailSettingsData& dst);
void fromApiToResource(const nx::vms::api::EmailSettingsData& src, QnEmailSettings& dst);

void fromApiToResource(const nx::vms::api::LayoutItemData& src, QnLayoutItemData& dst);
void fromResourceToApi(const QnLayoutItemData& src, nx::vms::api::LayoutItemData& dst);

void fromApiToResource(const nx::vms::api::LayoutData& src, QnLayoutResourcePtr& dst);
void fromResourceToApi(const QnLayoutResourcePtr& src, nx::vms::api::LayoutData& dst);
void fromApiToResourceList(const nx::vms::api::LayoutDataList& src, QnResourceList& dst);
void fromApiToResourceList(const nx::vms::api::LayoutDataList& src, QnLayoutResourceList& dst);
void fromResourceListToApi(const QnLayoutResourceList& src, nx::vms::api::LayoutDataList& dst);

void fromResourceToApi(const QnLicensePtr& src, nx::vms::api::LicenseData& dst);
void fromApiToResource(const nx::vms::api::LicenseData& src, QnLicensePtr& dst);
void fromResourceToApi(const QnLicensePtr& src, nx::vms::api::DetailedLicenseData& dst);
void fromResourceListToApi(const QnLicenseList& src, nx::vms::api::LicenseDataList& dst);
void fromApiToResourceList(const nx::vms::api::LicenseDataList& src, QnLicenseList& dst);

void fromResourceToApi(const QnStorageResourcePtr& src, nx::vms::api::StorageData& dst);
void fromApiToResource(const nx::vms::api::StorageData& src, QnStorageResourcePtr& dst);
void fromResourceListToApi(const QnStorageResourceList& src, nx::vms::api::StorageDataList& dst);

void fromResourceToApi(const QnMediaServerResourcePtr& src, nx::vms::api::MediaServerData& dst);
void fromApiToResource(const nx::vms::api::MediaServerData& src, QnMediaServerResourcePtr& dst);
void fromApiToResourceList(
    const nx::vms::api::MediaServerDataList& src,
    QnResourceList& dst,
    QnCommonModule* commonModule);
void fromApiToResourceList(
    const nx::vms::api::MediaServerDataList& src,
    QnMediaServerResourceList& dst,
    QnCommonModule* commonModule);
void fromResourceToApi(
    const QnMediaServerUserAttributesPtr& src,
    nx::vms::api::MediaServerUserAttributesData& dst);
void fromApiToResource(
    const nx::vms::api::MediaServerUserAttributesData& src,
    QnMediaServerUserAttributesPtr& dst);
void fromApiToResourceList(
    const nx::vms::api::MediaServerUserAttributesDataList& src,
    QnMediaServerUserAttributesList& dst);
void fromResourceListToApi(
    const QnMediaServerUserAttributesList& src,
    nx::vms::api::MediaServerUserAttributesDataList& dst);

void fromResourceToApi(const QnResourcePtr& src, nx::vms::api::ResourceData& dst);
void fromApiToResource(const nx::vms::api::ResourceData& src, QnResource* dst);

void fromApiToResource(const nx::vms::api::ResourceTypeData& src, QnResourceTypePtr& dst);
void fromApiToResourceList(const nx::vms::api::ResourceTypeDataList& src, QnResourceTypeList& dst);

QnUserResourcePtr fromApiToResource(
    const nx::vms::api::UserData& src, QnCommonModule* commonModule = nullptr);
void fromApiToResource(const nx::vms::api::UserData& src, QnUserResourcePtr& dst);
void fromResourceToApi(const QnUserResourcePtr& resource, nx::vms::api::UserData& data);
void fromApiToResourceList(const nx::vms::api::UserDataList& src, QnResourceList& dst);
void fromApiToResourceList(const nx::vms::api::UserDataList& src, QnUserResourceList& dst);

void fromApiToResource(const nx::vms::api::VideowallData& src, QnVideoWallResourcePtr& dst);
void fromResourceToApi(const QnVideoWallResourcePtr& src, nx::vms::api::VideowallData& dst);
void fromApiToResourceList(const nx::vms::api::VideowallDataList& src, QnResourceList& dst);
void fromApiToResourceList(const nx::vms::api::VideowallDataList& src, QnVideoWallResourceList& dst);

void fromApiToResource(
    const nx::vms::api::VideowallControlMessageData& data,
    QnVideoWallControlMessage& message);
void fromResourceToApi(
    const QnVideoWallControlMessage& message,
    nx::vms::api::VideowallControlMessageData& data);

void fromApiToResource(const nx::vms::api::WebPageData& src, QnWebPageResourcePtr& dst);
void fromResourceToApi(const QnWebPageResourcePtr& src, nx::vms::api::WebPageData& dst);
void fromApiToResourceList(const nx::vms::api::WebPageDataList& src, QnResourceList& dst);
void fromApiToResourceList(const nx::vms::api::WebPageDataList& src, QnWebPageResourceList& dst);

void fromApiToResource(
    const nx::vms::api::AnalyticsPluginData& src,
    nx::vms::common::AnalyticsPluginResourcePtr& dst);
void fromResourceToApi(
    const nx::vms::common::AnalyticsPluginResourcePtr& src,
    nx::vms::api::AnalyticsPluginData& dst);
void fromApiToResourceList(
    const nx::vms::api::AnalyticsPluginDataList& src,
    QnResourceList& dst);
void fromApiToResourceList(
    const nx::vms::api::AnalyticsPluginDataList& src,
    nx::vms::common::AnalyticsPluginResourceList& dst);

void fromApiToResource(
    const nx::vms::api::AnalyticsEngineData& src,
    nx::vms::common::AnalyticsEngineResourcePtr& dst);
void fromResourceToApi(
    const nx::vms::common::AnalyticsEngineResourcePtr& src,
    nx::vms::api::AnalyticsEngineData& dst);
void fromApiToResourceList(
    const nx::vms::api::AnalyticsEngineDataList& src,
    QnResourceList& dst);
void fromApiToResourceList(
    const nx::vms::api::AnalyticsEngineDataList& src,
    nx::vms::common::AnalyticsEngineResourceList& dst);

void deserializeNetAddrList(
    const QString& source,
    QList<nx::network::SocketAddress>& target,
    int defaultPort);

QList<nx::network::SocketAddress> moduleInformationEndpoints(
    const nx::vms::api::ModuleInformationWithAddresses& data);

void setModuleInformationEndpoints(
    nx::vms::api::ModuleInformationWithAddresses& data,
    const QList<nx::network::SocketAddress>& endpoints);

} // namespace ec2
