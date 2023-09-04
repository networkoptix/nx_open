// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/vms/api/data/saas_data.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::common::saas {

/**
 * @return True if the system described by the given systemContext is a SaaS system.
 */
NX_VMS_COMMON_API bool saasInitialized(SystemContext* systemContext);

/**
 * @return True if the system described by the given systemContext is a SaaS system and this system
 *     formally in the operational state, e.g. can use SaaS services. This includes suspended state
 *     as well.
 */
NX_VMS_COMMON_API bool saasServicesOperational(SystemContext* systemContext);

NX_VMS_COMMON_API bool localRecordingServicesOverused(SystemContext* systemContext);

NX_VMS_COMMON_API bool cloudStorageServicesOverused(SystemContext* systemContext);

NX_VMS_COMMON_API bool integrationServicesOverused(SystemContext* systemContext);

class NX_VMS_COMMON_API StringsHelper
{
    Q_DECLARE_TR_FUNCTIONS(StringsHelper)

public:
    /**
     * @return Human readable representation of the given SaaS state enumeration value.
     */
    static QString shortState(nx::vms::api::SaasState state);

    /**
     * @return Recommended action for a user if the given SaaS state is non-operational or
     *     requires attention.
     */
    static QString recommendedAction(nx::vms::api::SaasState state);
};

} // nx::vms::common::saas
