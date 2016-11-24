#pragma once

#include "cloud_connection_manager.h"
#include "network/auth/cdb_nonce_fetcher.h"
#include "network/auth/cloud_user_authenticator.h"

struct CloudManagerGroup
{
    CloudConnectionManager connectionManager;
    CdbNonceFetcher authenticationNonceFetcher;
    CloudUserAuthenticator userAuthenticator;

    CloudManagerGroup(AbstractNonceProvider* defaultNonceFetcher);
};
