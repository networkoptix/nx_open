#include "transport_security_manager.h"

#include <nx/cloud/cdb/client/cdb_request_path.h>

#include "../settings.h"

namespace nx::cdb {

TransportSecurityManager::TransportSecurityManager(
    const conf::Settings& settings)
    :
    m_settings(settings)
{
}

SecurityActions TransportSecurityManager::analyze(
    const nx::network::http::HttpServerConnection& /*connection*/,
    const nx::network::http::Request& request) const
{
    SecurityActions securityActions;

    // TODO: #ak Next time, when adding something here, replace with some general logic.
    // E.g., it can be based on stree.
    if (request.requestLine.url.path() == kAccountPasswordResetPath ||
        request.requestLine.url.path() == kAccountRegisterPath)
    {
        if (m_settings.accountManager().loginExistenceConcealDelay > std::chrono::milliseconds::zero())
            securityActions.responseDelay = m_settings.accountManager().loginExistenceConcealDelay;
    }

    return securityActions;
}

} // namespace nx::cdb
