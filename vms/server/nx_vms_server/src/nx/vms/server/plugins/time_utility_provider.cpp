#include "time_utility_provider.h"

#include <nx/sdk/uuid.h>
#include <plugins/plugin_container_api.h> //< for IID_TimeProvider
#include <utils/common/synctime.h>

namespace nx::vms::server::plugins {

using namespace nx::sdk;

IRefCountable* TimeUtilityProvider::queryInterface(InterfaceId id)
{
    // TODO: When nxpl::TimeProvider is deleted, move the value of its IID here to preserve binary
    // compatibility with old SDK plugins.
    return queryInterfaceSupportingDeprecatedId(id,
        Uuid(nxpl::IID_TimeProvider.bytes));
}

int64_t TimeUtilityProvider::vmsSystemTimeSinceEpochMs() const
{
    return qnSyncTime->currentMSecsSinceEpoch();
}

} // namespace nx::vms::server::plugins
