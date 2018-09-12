#include "security_manager.h"

namespace nx::cdb {

SecurityManager::SecurityManager(
    AuthenticationManager* authenticator,
    const AuthorizationManager& authorizer,
    const AccessBlocker& transportSecurityManager)
    :
    m_authenticator(authenticator),
    m_authorizer(authorizer),
    m_transportSecurityManager(transportSecurityManager)
{
}

AuthenticationManager& SecurityManager::authenticator()
{
    return *m_authenticator;
}

const AuthenticationManager& SecurityManager::authenticator() const
{
    return *m_authenticator;
}

const AccessBlocker& SecurityManager::transportSecurityManager() const
{
    return m_transportSecurityManager;
}

const AuthorizationManager& SecurityManager::authorizer() const
{
    return m_authorizer;
}

} // namespace nx::cdb
