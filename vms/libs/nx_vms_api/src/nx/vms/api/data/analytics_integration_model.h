// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

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

struct NX_VMS_API IntegrationModel
{
    /**%apidoc
     * Id of the Integration Resource, as a UUID.
     */
    nx::Uuid id;

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

    /**%apidoc
     * Integration type (whether the integration is an SDK or API one).
     */
    IntegrationType type = IntegrationType::sdk;

    IntegrationModel() = default;
    explicit IntegrationModel(AnalyticsPluginData data);

    using DbReadTypes = std::tuple<AnalyticsPluginData, ResourceParamWithRefData>;
    using DbListTypes = std::tuple<AnalyticsPluginDataList, ResourceParamWithRefDataList>;
    using DbUpdateTypes = std::tuple<AnalyticsPluginData>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<IntegrationModel> fromDbTypes(DbListTypes data);
};
#define nx_vms_api_analytics_IntegrationModel_Fields \
    (id)(integrationId)(name)(description)(version)(vendor)(type)
QN_FUSION_DECLARE_FUNCTIONS(IntegrationModel, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(IntegrationModel, nx_vms_api_analytics_IntegrationModel_Fields);

} // namespace nx::vms::api::analytics
