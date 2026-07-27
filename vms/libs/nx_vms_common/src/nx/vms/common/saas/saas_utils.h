// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/vms/api/data/saas_data.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::common::saas {

class ServiceManager;

/**
 * @return True if the service manager belongs to a SaaS system.
 */
NX_VMS_COMMON_API bool saasInitialized(const ServiceManager* saasServiceManager);
NX_VMS_COMMON_API bool saasInitialized(const SystemContext* systemContext);

/**
 * @return True if the service manager belongs to a SaaS system that is formally operational,
 *     including the suspended state.
 */
NX_VMS_COMMON_API bool saasServicesOperational(ServiceManager* saasServiceManager);

/**
 * @return True if the service manager belongs to an operational SaaS system where sending
 *     cross-site notifications is enabled.
 */
NX_VMS_COMMON_API bool crossSiteNotificationsAllowed(ServiceManager* saasServiceManager);

NX_VMS_COMMON_API bool localRecordingServicesOverused(ServiceManager* saasServiceManager);

NX_VMS_COMMON_API bool cloudStorageServicesOverused(ServiceManager* saasServiceManager);

NX_VMS_COMMON_API bool integrationServicesOverused(ServiceManager* saasServiceManager);

class NX_VMS_COMMON_API StringsHelper
{
    Q_DECLARE_TR_FUNCTIONS(StringsHelper)

public:
    /**
     * @return Recommended action for a user if the given SaaS state is non-operational or
     *     requires attention.
     */
    static QString recommendedAction(nx::vms::api::SaasState state);
};

} // nx::vms::common::saas
