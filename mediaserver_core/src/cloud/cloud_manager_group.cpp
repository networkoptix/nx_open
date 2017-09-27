#include "cloud_manager_group.h"

#include <nx/utils/std/cpp14.h>
#include <nx/utils/timer_manager.h>

#include <common/common_module.h>

#include "network/auth/cloud_user_info_pool.h"
#include "network/auth/generic_user_data_provider.h"

CloudManagerGroup::CloudManagerGroup(
    QnCommonModule* commonModule,
    AbstractNonceProvider* defaultNonceFetcher)
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
        std::make_unique<GenericUserDataProvider>(commonModule),
        authenticationNonceFetcher,
        cloudUserInfoPool)
{
}
