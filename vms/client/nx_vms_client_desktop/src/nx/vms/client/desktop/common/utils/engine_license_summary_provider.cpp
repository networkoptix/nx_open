// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine_license_summary_provider.h"

#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_service_usage_helper.h>
#include <nx/vms/time/formatter.h>

namespace nx::vms::client::desktop {

EngineLicenseSummaryProvider::EngineLicenseSummaryProvider(SystemContext* context, QObject* parent):
    QObject{parent},
    SystemContextAware{context}
{
}

QVariant EngineLicenseSummaryProvider::licenseSummary(QnUuid engineId) const
{
    if (const auto engine =
        resourcePool()->getResourceById<common::AnalyticsEngineResource>(engineId))
    {
        if (!engine->plugin()->manifest().isLicenseRequired)
            return {};

        auto summary = common::saas::IntegrationServiceUsageHelper{systemContext()}
            .info(engine->plugin()->manifest().id)
            .toVariantMap();

        const auto serviceStatus = systemContext()->saasServiceManager()->serviceStatus(
            api::SaasService::kAnalyticsIntegrationServiceType);
        if (serviceStatus.status == api::UseStatus::overUse)
        {
            summary["issueExpirationDate"] =
                time::toString(serviceStatus.issueExpirationDate.date(), time::Format::MMMM_d_yyyy);
        }

        return summary;
    }

    return {};
}

QVariant EngineLicenseSummaryProvider::licenseSummary(
    QnUuid engineId, QnUuid cameraId, const QVariantList& proposedEngines) const
{
    if (const auto engine =
        resourcePool()->getResourceById<common::AnalyticsEngineResource>(engineId))
    {
        if (!engine->plugin()->manifest().isLicenseRequired)
            return {};

        auto usageHelper = common::saas::IntegrationServiceUsageHelper{systemContext()};
        usageHelper.proposeChange(
            cameraId, nx::utils::toQSet(nx::utils::toTypedQList<QnUuid>(proposedEngines)));

        auto summary = usageHelper.info(engine->plugin()->manifest().id).toVariantMap();

        const auto serviceStatus = systemContext()->saasServiceManager()->serviceStatus(
            api::SaasService::kAnalyticsIntegrationServiceType);
        if (serviceStatus.status == api::UseStatus::overUse)
        {
            summary["issueExpirationDate"] =
                time::toString(serviceStatus.issueExpirationDate.date(), time::Format::MMMM_d_yyyy);
        }

        return summary;
    }

    return {};
}

QSet<QnUuid> EngineLicenseSummaryProvider::overusedEngines(
    QnUuid cameraId, const QSet<QnUuid>& proposedEngines) const
{
    auto usageHelper = common::saas::IntegrationServiceUsageHelper{systemContext()};
    usageHelper.proposeChange(cameraId, proposedEngines);

    if (!usageHelper.isOverflow())
        return {};

    QSet<QnUuid> result;
    for (const auto& engineId: proposedEngines)
    {
        const auto engine =
            resourcePool()->getResourceById<common::AnalyticsEngineResource>(engineId);
        if (!engine)
            continue;

        const auto summary = usageHelper.info(engine->plugin()->manifest().id);
        if (summary.inUse > summary.available)
            result.insert(engineId);
    }

    return result;
}

} // namespace nx::vms::client::desktop
