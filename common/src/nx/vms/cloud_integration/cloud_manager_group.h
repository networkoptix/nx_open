#pragma once

#include "cloud_connection_manager.h"
#include "cdb_nonce_fetcher.h"
#include "cloud_user_authenticator.h"
#include "cloud_user_info_pool.h"

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
        std::unique_ptr<auth::AbstractUserDataProvider> defaultAuthenticator);
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
