#include "security_manager.h"

namespace nx::cloud::db {

SecurityManager::SecurityManager(
    AuthenticationManager* authenticator,
    const AuthorizationManager& authorizer,
    const AccessBlocker& accessBlocker)
    :
    m_authenticator(authenticator),
    m_authorizer(authorizer),
    m_transportSecurityManager(accessBlocker)
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

const AccessBlocker& SecurityManager::accessBlocker() const
{
    return m_transportSecurityManager;
}

const AuthorizationManager& SecurityManager::authorizer() const
{
    return m_authorizer;
}

} // namespace nx::cloud::db
