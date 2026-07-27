// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_utils.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::common::saas {

using namespace nx::vms::api;

namespace {

bool servicesOverused(ServiceManager* saasServiceManager, const QString& seriviceType)
{
    if (!NX_ASSERT(saasServiceManager))
        return false;

    if (saasServiceManager->saasState() == SaasState::uninitialized)
        return false;

    return saasServiceManager->serviceStatus(seriviceType).status
        == nx::vms::api::UseStatus::overUse;
}

} // namespace

bool saasInitialized(const ServiceManager* saasServiceManager)
{
    return saasServiceManager->saasState() != SaasState::uninitialized;
}

bool saasInitialized(const SystemContext* systemContext)
{
    return saasInitialized(systemContext->saasServiceManager());
}

bool saasServicesOperational(ServiceManager* saasServiceManager)
{
    return saasServiceManager->saasServiceOperational();
}

bool crossSiteNotificationsAllowed(ServiceManager* saasServiceManager)
{
    return saasServicesOperational(saasServiceManager)
        && saasServiceManager->hasFeature(nx::vms::api::SaasTierLimitName::crossSiteAllowed);
}

bool localRecordingServicesOverused(ServiceManager* saasServiceManager)
{
    return servicesOverused(
        saasServiceManager,
        nx::vms::api::SaasService::kLocalRecordingServiceType);
}

bool cloudStorageServicesOverused(ServiceManager* saasServiceManager)
{
    return servicesOverused(
        saasServiceManager,
        nx::vms::api::SaasService::kCloudRecordingType);
}

bool integrationServicesOverused(ServiceManager* saasServiceManager)
{
    return servicesOverused(
        saasServiceManager,
        nx::vms::api::SaasService::kAnalyticsIntegrationServiceType);
}

QString StringsHelper::recommendedAction(nx::vms::api::SaasState state)
{
    using namespace nx::vms::api;
    switch (state)
    {
        case SaasState::uninitialized:
        case SaasState::active:
            return QString();

        case SaasState::suspended:
        case SaasState::shutdown:
            return tr("Contact your channel partner for assistance.");

        case SaasState::autoShutdown:
            return tr("Check internet connection between VMS and license server.");

        default:
            NX_ASSERT(false);
            return QString();
    }
}

} // nx::vms::common::saas
