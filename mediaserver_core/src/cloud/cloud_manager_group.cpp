#include "cloud_manager_group.h"

#include <nx/utils/std/cpp14.h>

#include "network/auth/generic_user_data_provider.h"

CloudManagerGroup::CloudManagerGroup(AbstractNonceProvider* defaultNonceFetcher):
    authenticationNonceFetcher(&connectionManager, defaultNonceFetcher),
    userAuthenticator(
        &connectionManager,
        std::make_unique<GenericUserDataProvider>(),
        authenticationNonceFetcher)
{
}
