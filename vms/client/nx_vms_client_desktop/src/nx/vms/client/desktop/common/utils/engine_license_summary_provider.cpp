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

QVariant EngineLicenseSummaryProvider::licenseSummary(nx::Uuid engineId) const
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
    nx::Uuid engineId,
    QnResource* camera,
    const QVariantList& proposedEngines) const
{
    if (const auto engine =
        resourcePool()->getResourceById<common::AnalyticsEngineResource>(engineId))
    {
        if (!engine->plugin()->manifest().isLicenseRequired)
            return {};

        auto usageHelper = common::saas::IntegrationServiceUsageHelper{systemContext()};
        if (NX_ASSERT(camera))
        {
            usageHelper.proposeChange(
                camera->getId(),
                nx::utils::toQSet(nx::utils::toTypedQList<nx::Uuid>(proposedEngines)));
        }

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

QSet<nx::Uuid> EngineLicenseSummaryProvider::overusedEngines(
    QnResource* camera,
    const QSet<nx::Uuid>& proposedEngines) const
{
    auto usageHelper = common::saas::IntegrationServiceUsageHelper{systemContext()};
    if (NX_ASSERT(camera))
        usageHelper.proposeChange(camera->getId(), proposedEngines);

    if (!usageHelper.isOverflow())
        return {};

    QSet<nx::Uuid> result;
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
