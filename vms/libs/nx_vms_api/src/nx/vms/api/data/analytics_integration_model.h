// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <optional>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/analytics_data.h>

namespace nx::vms::api::analytics {

extern NX_VMS_API const QString kPluginManifestProperty;
extern NX_VMS_API const QString kIntegrationTypeProperty;

NX_REFLECTION_ENUM_CLASS(IntegrationType,
    sdk,
    api
);

struct NX_VMS_API CommonInfo
{
    /**%apidoc
     * Id provided by the Integration, in a human-readable format.
     * %example "my.awesome.integration"
     */
    QString integrationId;

    /**%apidoc
     * Name of the Integration.
     * %example "My Awesome Integration"
     */
    QString name;

    /**%apidoc
     * Information about the Integration.
     * %example "This Integration does awesome things!"
     */
    QString description;

    /**%apidoc
     * Integration version.
     * %example "1.0.1"
     */
    QString version;

    /**%apidoc
     * Integration vendor.
     * %example "My Company"
     */
    QString vendor;
};
#define nx_vms_api_analytics_CommonInfo_Fields \
    (integrationId)(name)(description)(version)(vendor)
QN_FUSION_DECLARE_FUNCTIONS(CommonInfo, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(CommonInfo, nx_vms_api_analytics_CommonInfo_Fields);

struct NX_VMS_API SdkIntegrationInfo: CommonInfo
{
    /**%apidoc
     * Information about the SDK Plugin.
     */
    nx::vms::api::PluginInfo pluginInfo;

    SdkIntegrationInfo() = default;
    SdkIntegrationInfo(const CommonInfo& commonInfo):
        CommonInfo(commonInfo)
    {
    }
};
#define nx_vms_api_analytics_SdkIntegrationInfo_Fields \
    nx_vms_api_analytics_CommonInfo_Fields \
    (pluginInfo)
QN_FUSION_DECLARE_FUNCTIONS(SdkIntegrationInfo, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(SdkIntegrationInfo, nx_vms_api_analytics_SdkIntegrationInfo_Fields);

struct NX_VMS_API ApiIntegrationInfo: CommonInfo
{
    /**%apidoc
     * Whether the API Integration is online.
     */
    bool isOnline = false;

    /**%apidoc
     * Id of the Integration User.
     */
    nx::Uuid integrationUserId;

    ApiIntegrationInfo() = default;
    ApiIntegrationInfo(const CommonInfo& commonInfo):
        CommonInfo(commonInfo)
    {
    }
};
#define nx_vms_api_analytics_ApiIntegrationInfo_Fields \
    nx_vms_api_analytics_CommonInfo_Fields \
    (isOnline)(integrationUserId)
QN_FUSION_DECLARE_FUNCTIONS(ApiIntegrationInfo, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(ApiIntegrationInfo, nx_vms_api_analytics_ApiIntegrationInfo_Fields);

struct NX_VMS_API IntegrationModel
{
    /**%apidoc
     * Id of the Integration Resource, as a UUID.
     */
    nx::Uuid id;

    /**%apidoc
     * Integration type (whether the integration is an SDK or API one).
     */
    IntegrationType integrationType = IntegrationType::sdk;

    /**%apidoc
     * SDK Integration information.
     */
    std::optional<SdkIntegrationInfo> sdkIntegrationInfo;

    /**%apidoc
     * API Integration information.
     */
    std::optional<ApiIntegrationInfo> apiIntegrationInfo;
};
#define nx_vms_api_analytics_IntegrationModel_Fields \
    (id)(integrationType)(sdkIntegrationInfo)(apiIntegrationInfo)
QN_FUSION_DECLARE_FUNCTIONS(IntegrationModel, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(IntegrationModel, nx_vms_api_analytics_IntegrationModel_Fields);

} // namespace nx::vms::api::analytics
