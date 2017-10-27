#pragma once

#include <chrono>

#include "cloud_connection_manager.h"
#include "cdb_nonce_fetcher.h"
#include "cloud_user_authenticator.h"
#include "cloud_user_info_pool.h"
#include "master_server_status_watcher.h"
#include "connect_to_cloud_watcher.h"

namespace nx {
namespace vms {
namespace cloud_integration {

struct CloudManagerGroup
{
    CloudConnectionManager connectionManager;
    CloudUserInfoPool cloudUserInfoPool;
    CdbNonceFetcher authenticationNonceFetcher;
    CloudUserAuthenticator userAuthenticator;

    CloudManagerGroup(
        QnCommonModule* commonModule,
        auth::AbstractNonceProvider* defaultNonceFetcher,
        AbstractEc2CloudConnector* ec2CloudConnector,
        std::unique_ptr<auth::AbstractUserDataProvider> defaultAuthenticator,
        std::chrono::milliseconds delayBeforeSettingMasterFlag);

    void setCloudDbUrl(const QUrl& cdbUrl);

private:
    QnMasterServerStatusWatcher m_masterServerStatusWatcher;
    QnConnectToCloudWatcher m_connectToCloudWatcher;
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
