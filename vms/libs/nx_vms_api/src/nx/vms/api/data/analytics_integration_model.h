// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API AnalyticsIntegrationModel
{
    /**%apidoc
     * Id of the Integration Resource, as a UUID.
     * %example {cef375cf-5ad9-42c1-8bd2-4f12b3cb47d0}
     */
    QnUuid id;

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

    QnUuid getId() const { return id; }

    AnalyticsIntegrationModel() = default;
    explicit AnalyticsIntegrationModel(AnalyticsPluginData data);

    using DbReadTypes = std::tuple<AnalyticsPluginData, ResourceParamWithRefData>;
    using DbListTypes = std::tuple<AnalyticsPluginDataList, ResourceParamWithRefDataList>;
    using DbUpdateTypes = std::tuple<AnalyticsPluginData>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<AnalyticsIntegrationModel> fromDbTypes(DbListTypes data);
    static AnalyticsIntegrationModel fromDb(AnalyticsPluginData data);
};
#define nx_vms_api_AnalyticsIntegrationModel_Fields \
    (id)(integrationId)(name)(description)(version)(vendor)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsIntegrationModel, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(AnalyticsIntegrationModel, nx_vms_api_AnalyticsIntegrationModel_Fields);

} // namespace nx::vms::api
