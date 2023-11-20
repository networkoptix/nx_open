// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/api/data/webpage_data.h>
#include <nx/vms/event/event_fwd.h>
#include <nx_ec/ec_api_fwd.h>
#include <utils/email/email_fwd.h>

#include "api_globals.h"

namespace nx { namespace network { class SocketAddress; }}

namespace ec2 {

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::EventRuleData& src,
    nx::vms::event::RulePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const nx::vms::event::RulePtr& src,
    nx::vms::api::EventRuleData& dst);
NX_VMS_COMMON_API void fromApiToResourceList(
    const nx::vms::api::EventRuleDataList& src,
    nx::vms::event::RuleList& dst);
NX_VMS_COMMON_API void fromResourceListToApi(
    const nx::vms::event::RuleList& src,
    nx::vms::api::EventRuleDataList& dst);

NX_VMS_COMMON_API void fromResourceToApi(
    const nx::vms::event::AbstractActionPtr& src,
    nx::vms::api::EventActionData& dst);
NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::EventActionData& src,
    nx::vms::event::AbstractActionPtr& dst);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::CameraData& src,
    QnVirtualCameraResourcePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const QnVirtualCameraResourcePtr& src,
    nx::vms::api::CameraData& dst);
NX_VMS_COMMON_API void fromResourceListToApi(
    const QnVirtualCameraResourceList& src,
    nx::vms::api::CameraDataList& dst);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::CameraDataEx& src,
    QnVirtualCameraResourcePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const QnVirtualCameraResourcePtr& src,
    nx::vms::api::CameraDataEx& dst);
NX_VMS_COMMON_API void fromResourceListToApi(
    const QnVirtualCameraResourceList& src,
    nx::vms::api::CameraDataExList& dst);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::LayoutItemData& src,
    nx::vms::common::LayoutItemData& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const nx::vms::common::LayoutItemData& src,
    nx::vms::api::LayoutItemData& dst);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::LayoutData& src,
    QnLayoutResourcePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const QnLayoutResourcePtr& src,
    nx::vms::api::LayoutData& dst);

NX_VMS_COMMON_API void fromResourceToApi(const QnLicensePtr& src, nx::vms::api::LicenseData& dst);
NX_VMS_COMMON_API void fromApiToResource(const nx::vms::api::LicenseData& src, QnLicensePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const QnLicensePtr& src,
    nx::vms::api::DetailedLicenseData& dst);
NX_VMS_COMMON_API void fromResourceListToApi(
    const QnLicenseList& src,
    nx::vms::api::LicenseDataList& dst);
NX_VMS_COMMON_API void fromApiToResourceList(
    const nx::vms::api::LicenseDataList& src,
    QnLicenseList& dst);

NX_VMS_COMMON_API void fromResourceToApi(
    const QnStorageResourcePtr& src,
    nx::vms::api::StorageData& dst);
NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::StorageData& src,
    QnStorageResourcePtr& dst);
NX_VMS_COMMON_API void fromResourceListToApi(
    const QnStorageResourceList& src,
    nx::vms::api::StorageDataList& dst);

NX_VMS_COMMON_API void fromResourceToApi(
    const QnMediaServerResourcePtr& src,
    nx::vms::api::MediaServerData& dst);
NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::MediaServerData& src,
    QnMediaServerResourcePtr& dst);

NX_VMS_COMMON_API void fromResourceToApi(const QnResourcePtr& src, nx::vms::api::ResourceData& dst);
NX_VMS_COMMON_API void fromApiToResource(const nx::vms::api::ResourceData& src, QnResource* dst);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::ResourceTypeData& src,
    QnResourceTypePtr& dst);
NX_VMS_COMMON_API void fromApiToResourceList(
    const nx::vms::api::ResourceTypeDataList& src,
    QnResourceTypeList& dst);

NX_VMS_COMMON_API QnUserResourcePtr fromApiToResource(const nx::vms::api::UserData& src);
NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::UserData& src, const QnUserResourcePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const QnUserResource& resource, nx::vms::api::UserData& data);
NX_VMS_COMMON_API void fromResourceToApi(
    const QnUserResourcePtr& resource, nx::vms::api::UserData& data);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::VideowallData& src,
    QnVideoWallResourcePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const QnVideoWallResourcePtr& src,
    nx::vms::api::VideowallData& dst);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::VideowallControlMessageData& data,
    QnVideoWallControlMessage& message);
NX_VMS_COMMON_API void fromResourceToApi(
    const QnVideoWallControlMessage& message,
    nx::vms::api::VideowallControlMessageData& data);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::WebPageData& src,
    QnWebPageResourcePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const QnWebPageResourcePtr& src,
    nx::vms::api::WebPageData& dst);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::AnalyticsPluginData& src,
    nx::vms::common::AnalyticsPluginResourcePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const nx::vms::common::AnalyticsPluginResourcePtr& src,
    nx::vms::api::AnalyticsPluginData& dst);

NX_VMS_COMMON_API void fromApiToResource(
    const nx::vms::api::AnalyticsEngineData& src,
    nx::vms::common::AnalyticsEngineResourcePtr& dst);
NX_VMS_COMMON_API void fromResourceToApi(
    const nx::vms::common::AnalyticsEngineResourcePtr& src,
    nx::vms::api::AnalyticsEngineData& dst);

NX_VMS_COMMON_API void deserializeNetAddrList(
    const QString& source,
    QList<nx::network::SocketAddress>& target,
    int defaultPort);

NX_VMS_COMMON_API QList<nx::network::SocketAddress> moduleInformationEndpoints(
    const nx::vms::api::ModuleInformationWithAddresses& data);

NX_VMS_COMMON_API void setModuleInformationEndpoints(
    nx::vms::api::ModuleInformationWithAddresses& data,
    const QList<nx::network::SocketAddress>& endpoints);

} // namespace ec2
