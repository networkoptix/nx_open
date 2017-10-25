#include "cloud_manager_group.h"

#include <memory>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/timer_manager.h>

#include <common/common_module.h>

namespace nx {
namespace vms {
namespace cloud_integration {

CloudManagerGroup::CloudManagerGroup(
    QnCommonModule* commonModule,
    auth::AbstractNonceProvider* defaultNonceFetcher,
    std::unique_ptr<auth::AbstractUserDataProvider> defaultAuthenticator)
:
    connectionManager(commonModule),
    cloudUserInfoPool(
        std::make_unique<CloudUserInfoPoolSupplier>(commonModule->resourcePool())),
    authenticationNonceFetcher(
        nx::utils::TimerManager::instance(),
        &connectionManager,
        &cloudUserInfoPool,
        defaultNonceFetcher),
    userAuthenticator(
        &connectionManager,
        std::move(defaultAuthenticator),
        authenticationNonceFetcher,
        cloudUserInfoPool)
{
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
