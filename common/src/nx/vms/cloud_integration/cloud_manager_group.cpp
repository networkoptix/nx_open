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
    AbstractEc2CloudConnector* ec2CloudConnector,
    std::unique_ptr<auth::AbstractUserDataProvider> defaultAuthenticator,
    std::chrono::milliseconds delayBeforeSettingMasterFlag)
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
        cloudUserInfoPool),
    m_masterServerStatusWatcher(commonModule, delayBeforeSettingMasterFlag), // TODO: #ak Take from some settings.
    m_connectToCloudWatcher(commonModule, ec2CloudConnector)
{
}

void CloudManagerGroup::setCloudDbUrl(const nx::utils::Url& cdbUrl)
{
    connectionManager.setCloudDbUrl(cdbUrl);
    m_connectToCloudWatcher.setCloudDbUrl(cdbUrl);
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
